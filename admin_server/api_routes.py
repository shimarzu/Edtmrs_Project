"""
EDTMRS - API Routes
All REST endpoints for the admin server
"""

import logging
from datetime import datetime
from typing import Optional
from fastapi import APIRouter, Depends, HTTPException, Request, Response
import aiosqlite

from database import DB_PATH, get_db
from auth import (
    verify_password, create_access_token, get_current_user,
    require_admin, hash_password
)
from models import (
    LoginRequest, DeviceEventPayload, BlockDeviceRequest,
    WhitelistDeviceRequest, IsolateEndpointRequest,
    AcknowledgeAlertRequest, CreateUserRequest
)
from websocket_manager import manager

router = APIRouter()
logger = logging.getLogger(__name__)


# ─── AUTH ─────────────────────────────────────────────────────────────────────

@router.post("/auth/login")
async def login(payload: LoginRequest):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute(
            "SELECT * FROM users WHERE username = ? AND is_active = 1",
            (payload.username,)
        )
        user = await cur.fetchone()

    if not user or not verify_password(payload.password, user["password_hash"]):
        raise HTTPException(status_code=401, detail="Invalid credentials")

    token = create_access_token({"sub": str(user["id"]), "role": user["role"]})
    return {
        "access_token": token,
        "token_type": "bearer",
        "user": {
            "id": user["id"],
            "username": user["username"],
            "email": user["email"],
            "role": user["role"]
        }
    }


@router.get("/auth/me")
async def me(current_user=Depends(get_current_user)):
    return current_user


# ─── DEVICE EVENT (from C++ agents) ──────────────────────────────────────────

@router.post("/device-event")
async def receive_device_event(payload: DeviceEventPayload, request: Request):
    """Endpoint agents POST device events here."""
    client_ip = request.client.host

    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row

        # Upsert endpoint
        await db.execute("""
            INSERT INTO endpoints (hostname, ip_address, username, status, last_seen, agent_version)
            VALUES (?, ?, ?, 'online', CURRENT_TIMESTAMP, ?)
            ON CONFLICT(hostname) DO UPDATE SET
                ip_address = excluded.ip_address,
                username = excluded.username,
                status = 'online',
                last_seen = CURRENT_TIMESTAMP,
                agent_version = excluded.agent_version
        """, (payload.hostname, client_ip, payload.username, payload.agent_version))
        await db.commit()

        # Create unique constraint workaround for endpoints
        cur = await db.execute("SELECT id FROM endpoints WHERE hostname = ?", (payload.hostname,))
        ep = await cur.fetchone()
        endpoint_id = ep["id"] if ep else None

        # Determine risk level
        risk_level = await classify_device_risk(db, payload)

        # ── UPSERT device log ─────────────────────────────────────────────────
        # Same USB on same endpoint = UPDATE existing record (increment counter)
        # New USB never seen before = INSERT new record
        # Uniqueness is determined by: vendor_id + product_id + hostname
        # (serial_number alone is unreliable — many cheap USBs report "unknown")

        # Always try to find existing record for this device on this endpoint
        # Match by: vendor_id + product_id + hostname (most reliable combination)
        # Fallback: device_name + hostname (for devices with unknown VID)
        existing = None
        if payload.vendor_id not in ("unknown", "", None):
            cur = await db.execute("""
                SELECT id FROM device_logs
                WHERE vendor_id = ? AND product_id = ? AND hostname = ?
                LIMIT 1
            """, (payload.vendor_id, payload.product_id, payload.hostname))
            existing = await cur.fetchone()

        if not existing:
            # Fallback: match by device name + hostname
            cur = await db.execute("""
                SELECT id FROM device_logs
                WHERE device_name = ? AND hostname = ?
                LIMIT 1
            """, (payload.device_name, payload.hostname))
            existing = await cur.fetchone()

        if existing:
            # Device seen before on this endpoint — just update the timestamp
            # and increment the connection counter. Do NOT create a new row.
            log_id = existing["id"]
            await db.execute("""
                UPDATE device_logs SET
                    last_seen     = CURRENT_TIMESTAMP,
                    connect_count = connect_count + 1,
                    drive_letter  = ?,
                    username      = ?,
                    risk_level    = ?,
                    status        = 'detected'
                WHERE id = ?
            """, (payload.drive_letter, payload.username, risk_level, log_id))
            await db.commit()
            is_new_device = False
        else:
            # First time this device is seen — create a new record
            cur = await db.execute("""
                INSERT INTO device_logs
                    (endpoint_id, vendor_id, product_id, serial_number, device_name,
                     device_type, drive_letter, hostname, username, risk_level, status,
                     connect_count)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, 'detected', 1)
            """, (
                endpoint_id, payload.vendor_id, payload.product_id,
                payload.serial_number, payload.device_name, payload.device_type,
                payload.drive_letter, payload.hostname, payload.username, risk_level
            ))
            await db.commit()
            log_id = cur.lastrowid
            is_new_device = True

        # Create alert only for:
        # 1. NEW devices (first time seen) with medium/high/critical risk
        # 2. ANY re-connection of a CRITICAL/blocked device
        alert_id = None
        if (is_new_device and risk_level in ("medium", "high", "critical")) or            (not is_new_device and risk_level == "critical"):
            severity_map = {"medium": "medium", "high": "high", "critical": "critical"}
            severity = severity_map.get(risk_level, "medium")
            message = build_alert_message(payload, risk_level)

            cur2 = await db.execute("""
                INSERT INTO alerts (device_log_id, endpoint_id, alert_type, severity, message)
                VALUES (?, ?, 'device_connected', ?, ?)
            """, (log_id, endpoint_id, severity, message))
            await db.commit()
            alert_id = cur2.lastrowid

    # Broadcast via WebSocket
    event_data = {
        "id": log_id,
        "hostname": payload.hostname,
        "username": payload.username,
        "device_name": payload.device_name,
        "vendor_id": payload.vendor_id,
        "product_id": payload.product_id,
        "serial_number": payload.serial_number,
        "drive_letter": payload.drive_letter,
        "device_type": payload.device_type,
        "risk_level": risk_level,
        "timestamp": datetime.utcnow().isoformat(),
        "ip_address": client_ip
    }
    await manager.send_device_event(event_data)

    if alert_id:
        await manager.send_alert({
            "id": alert_id,
            "device_log_id": log_id,
            "hostname": payload.hostname,
            "device_name": payload.device_name,
            "risk_level": risk_level,
            "message": build_alert_message(payload, risk_level),
            "timestamp": datetime.utcnow().isoformat()
        })

    logger.info(f"Device event from {payload.hostname}: {payload.device_name} [{risk_level}]")

    # Determine action to send back to agent
    # Agent will physically block/allow based on this
    action = "none"
    if risk_level == "critical":
        action = "block"
    elif risk_level == "safe":
        action = "allow"

    return {
        "status": "ok",
        "log_id": log_id,
        "risk_level": risk_level,
        "action": action        # Agent reads this and physically blocks/allows
    }


async def classify_device_risk(db, payload: DeviceEventPayload) -> str:
    """Classify device risk: safe / medium / high / critical.
    
    Classification criteria (from proposal Section 3.2 Step 7):
    a) Unknown device identifiers → HIGH
    b) Non-whitelisted serial numbers → MEDIUM
    c) Suspicious activity patterns (same device 3+ times in 10 min) → CRITICAL
    d) Blocked devices → CRITICAL
    e) Whitelisted devices → SAFE
    """
    # Check whitelist first
    cur = await db.execute("""
        SELECT id FROM device_whitelist
        WHERE (serial_number = ? AND serial_number != 'unknown')
           OR (vendor_id = ? AND product_id = ? AND vendor_id != 'unknown')
    """, (payload.serial_number, payload.vendor_id, payload.product_id))
    if await cur.fetchone():
        return "safe"

    # Check blocked list
    cur = await db.execute("""
        SELECT id FROM blocked_devices
        WHERE serial_number = ? OR (vendor_id = ? AND product_id = ?)
    """, (payload.serial_number, payload.vendor_id, payload.product_id))
    if await cur.fetchone():
        return "critical"

    # Check suspicious activity pattern:
    # Same VID+PID connecting 3+ times in the last 10 minutes = suspicious
    if payload.vendor_id not in ("unknown", "", None):
        cur = await db.execute("""
            SELECT COUNT(*) as cnt FROM device_logs
            WHERE vendor_id = ? AND product_id = ?
              AND hostname = ?
              AND timestamp >= DATETIME('now', '-10 minutes')
        """, (payload.vendor_id, payload.product_id, payload.hostname))
        row = await cur.fetchone()
        if row and row["cnt"] >= 2:
            return "critical"

    # Unknown identifiers = high risk
    if payload.serial_number in ("unknown", "", None):
        return "high"
    if payload.vendor_id in ("unknown", "", None):
        return "medium"

    return "medium"


def build_alert_message(payload: DeviceEventPayload, risk: str) -> str:
    return (
        f"[{risk.upper()}] Device '{payload.device_name}' connected to {payload.hostname} "
        f"by user '{payload.username}'. "
        f"VID:{payload.vendor_id} PID:{payload.product_id} Serial:{payload.serial_number}"
    )


# ─── DASHBOARD STATS ─────────────────────────────────────────────────────────

@router.get("/stats")
async def get_stats(current_user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row

        total_endpoints = (await (await db.execute("SELECT COUNT(*) as c FROM endpoints")).fetchone())["c"]
        online_endpoints = (await (await db.execute("SELECT COUNT(*) as c FROM endpoints WHERE status='online'")).fetchone())["c"]
        total_devices = (await (await db.execute("SELECT COUNT(*) as c FROM device_logs")).fetchone())["c"]
        suspicious = (await (await db.execute("SELECT COUNT(*) as c FROM device_logs WHERE risk_level IN ('high','critical')")).fetchone())["c"]
        unack_alerts = (await (await db.execute("SELECT COUNT(*) as c FROM alerts WHERE is_acknowledged=0")).fetchone())["c"]
        total_alerts = (await (await db.execute("SELECT COUNT(*) as c FROM alerts")).fetchone())["c"]

        # Activity last 7 days
        cur = await db.execute("""
            SELECT DATE(timestamp) as day, COUNT(*) as count
            FROM device_logs
            WHERE timestamp >= DATETIME('now', '-7 days')
            GROUP BY day ORDER BY day
        """)
        activity = [dict(r) for r in await cur.fetchall()]

    return {
        "total_endpoints": total_endpoints,
        "online_endpoints": online_endpoints,
        "total_devices": total_devices,
        "suspicious_devices": suspicious,
        "unacknowledged_alerts": unack_alerts,
        "total_alerts": total_alerts,
        "activity_7d": activity
    }


# ─── DEVICES ─────────────────────────────────────────────────────────────────

@router.get("/devices")
async def get_devices(
    search: Optional[str] = None,
    risk_level: Optional[str] = None,
    limit: int = 100,
    offset: int = 0,
    current_user=Depends(get_current_user)
):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        query = "SELECT * FROM device_logs WHERE 1=1"
        params = []
        if search:
            query += " AND (device_name LIKE ? OR hostname LIKE ? OR serial_number LIKE ? OR vendor_id LIKE ?)"
            s = f"%{search}%"
            params += [s, s, s, s]
        if risk_level:
            query += " AND risk_level = ?"
            params.append(risk_level)
        query += " ORDER BY timestamp DESC LIMIT ? OFFSET ?"
        params += [limit, offset]
        cur = await db.execute(query, params)
        rows = [dict(r) for r in await cur.fetchall()]

        count_query = "SELECT COUNT(*) as c FROM device_logs WHERE 1=1"
        count_params = []
        if search:
            count_query += " AND (device_name LIKE ? OR hostname LIKE ? OR serial_number LIKE ? OR vendor_id LIKE ?)"
            s = f"%{search}%"
            count_params += [s, s, s, s]
        if risk_level:
            count_query += " AND risk_level = ?"
            count_params.append(risk_level)
        total = (await (await db.execute(count_query, count_params)).fetchone())["c"]

    return {"devices": rows, "total": total}


# ─── ENDPOINTS ────────────────────────────────────────────────────────────────

@router.get("/endpoints")
async def get_endpoints(current_user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM endpoints ORDER BY last_seen DESC")
        rows = [dict(r) for r in await cur.fetchall()]

        # Mark stale endpoints as offline (no heartbeat in 5 min)
        await db.execute("""
            UPDATE endpoints SET status = 'offline'
            WHERE last_seen < DATETIME('now', '-5 minutes') AND status = 'online'
        """)
        await db.commit()

    return {"endpoints": rows, "total": len(rows)}


@router.post("/isolate-endpoint")
async def isolate_endpoint(payload: IsolateEndpointRequest, current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        await db.execute(
            "UPDATE endpoints SET is_isolated = 1 WHERE id = ?",
            (payload.endpoint_id,)
        )
        await db.commit()
        cur = await db.execute("SELECT hostname FROM endpoints WHERE id=?", (payload.endpoint_id,))
        row = await cur.fetchone()

    hostname = row["hostname"] if row else "unknown"
    await manager.broadcast({
        "type": "endpoint_isolated",
        "data": {"endpoint_id": payload.endpoint_id, "hostname": hostname, "reason": payload.reason}
    })
    return {"status": "ok", "message": f"Endpoint {payload.endpoint_id} isolated"}


@router.post("/unisolate-endpoint/{endpoint_id}")
async def unisolate_endpoint(endpoint_id: int, current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("UPDATE endpoints SET is_isolated = 0 WHERE id = ?", (endpoint_id,))
        await db.commit()
    return {"status": "ok", "message": f"Endpoint {endpoint_id} unisolated"}


# ─── ALERTS ──────────────────────────────────────────────────────────────────

@router.get("/alerts")
async def get_alerts(
    acknowledged: Optional[bool] = None,
    severity: Optional[str] = None,
    limit: int = 100,
    offset: int = 0,
    current_user=Depends(get_current_user)
):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        query = """
            SELECT a.*, d.device_name, d.vendor_id, d.product_id,
                   d.serial_number, d.hostname, d.username, d.drive_letter
            FROM alerts a
            LEFT JOIN device_logs d ON a.device_log_id = d.id
            WHERE 1=1
        """
        params = []
        if acknowledged is not None:
            query += " AND a.is_acknowledged = ?"
            params.append(1 if acknowledged else 0)
        if severity:
            query += " AND a.severity = ?"
            params.append(severity)
        query += " ORDER BY a.created_at DESC LIMIT ? OFFSET ?"
        params += [limit, offset]
        cur = await db.execute(query, params)
        rows = [dict(r) for r in await cur.fetchall()]
        total = len(rows)

    return {"alerts": rows, "total": total}


@router.post("/alerts/acknowledge")
async def acknowledge_alert(payload: AcknowledgeAlertRequest, current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("""
            UPDATE alerts SET is_acknowledged=1, acknowledged_by=?, acknowledged_at=CURRENT_TIMESTAMP
            WHERE id=?
        """, (current_user["id"], payload.alert_id))
        await db.commit()
    return {"status": "ok"}


@router.post("/alerts/acknowledge-all")
async def acknowledge_all_alerts(current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("""
            UPDATE alerts SET is_acknowledged=1, acknowledged_by=?, acknowledged_at=CURRENT_TIMESTAMP
            WHERE is_acknowledged=0
        """, (current_user["id"],))
        await db.commit()
    return {"status": "ok"}


# ─── BLOCK / WHITELIST ────────────────────────────────────────────────────────

@router.post("/block-device")
async def block_device(payload: BlockDeviceRequest, current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("""
            INSERT INTO blocked_devices (vendor_id, product_id, serial_number, device_name, blocked_by, reason)
            VALUES (?, ?, ?, ?, ?, ?)
        """, (payload.vendor_id, payload.product_id, payload.serial_number,
              payload.device_name, current_user["id"], payload.reason))

        # Also update risk level for existing logs
        await db.execute("""
            UPDATE device_logs SET risk_level='critical', status='blocked'
            WHERE serial_number=? OR (vendor_id=? AND product_id=?)
        """, (payload.serial_number, payload.vendor_id, payload.product_id))
        await db.commit()

    return {"status": "ok", "message": "Device blocked"}


@router.get("/blocked-devices")
async def get_blocked_devices(current_user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM blocked_devices ORDER BY created_at DESC")
        rows = [dict(r) for r in await cur.fetchall()]
    return {"blocked_devices": rows}


@router.post("/whitelist-device")
async def whitelist_device(payload: WhitelistDeviceRequest, current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("""
            INSERT INTO device_whitelist (vendor_id, product_id, serial_number, device_name, added_by, notes)
            VALUES (?, ?, ?, ?, ?, ?)
        """, (payload.vendor_id, payload.product_id, payload.serial_number,
              payload.device_name, current_user["id"], payload.notes))

        await db.execute("""
            UPDATE device_logs SET risk_level='safe', status='whitelisted'
            WHERE serial_number=? OR (vendor_id=? AND product_id=?)
        """, (payload.serial_number, payload.vendor_id, payload.product_id))
        await db.commit()

    return {"status": "ok", "message": "Device whitelisted"}


@router.get("/whitelist")
async def get_whitelist(current_user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM device_whitelist ORDER BY created_at DESC")
        rows = [dict(r) for r in await cur.fetchall()]
    return {"whitelist": rows}


@router.delete("/whitelist/{wl_id}")
async def remove_whitelist(wl_id: int, current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("DELETE FROM device_whitelist WHERE id=?", (wl_id,))
        await db.commit()
    return {"status": "ok"}


@router.delete("/blocked-devices/{bd_id}")
async def unblock_device(bd_id: int, current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("DELETE FROM blocked_devices WHERE id=?", (bd_id,))
        await db.commit()
    return {"status": "ok"}


# ─── USERS ────────────────────────────────────────────────────────────────────

@router.get("/users")
async def get_users(current_user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT id, username, email, role, is_active, created_at FROM users")
        rows = [dict(r) for r in await cur.fetchall()]
    return {"users": rows}


@router.post("/users")
async def create_user(payload: CreateUserRequest, current_user=Depends(require_admin)):
    hashed = hash_password(payload.password)
    async with aiosqlite.connect(DB_PATH) as db:
        try:
            await db.execute(
                "INSERT INTO users (username, email, password_hash, role) VALUES (?, ?, ?, ?)",
                (payload.username, payload.email, hashed, payload.role)
            )
            await db.commit()
        except Exception as e:
            raise HTTPException(status_code=400, detail=f"User creation failed: {str(e)}")
    return {"status": "ok", "message": "User created"}



# ─── CSV EXPORT ───────────────────────────────────────────────────────────────

@router.get("/export/devices")
async def export_devices_csv(current_user=Depends(get_current_user)):
    """Export all device logs as CSV for reporting."""
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute(
            "SELECT * FROM device_logs ORDER BY timestamp DESC"
        )
        rows = [dict(r) for r in await cur.fetchall()]

    # Build CSV content
    lines = ["ID,Device Name,Vendor ID,Product ID,Serial Number,Drive,Hostname,Username,Risk Level,Status,Timestamp"]
    for r in rows:
        lines.append(
            f"{r['id']},{r['device_name']},{r['vendor_id']},{r['product_id']},"
            f"{r['serial_number']},{r['drive_letter']},{r['hostname']},"
            f"{r['username']},{r['risk_level']},{r['status']},{r['timestamp']}"
        )
    csv_content = "\n".join(lines)

    return Response(
        content=csv_content,
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=edtmrs_device_logs.csv"}
    )


@router.get("/export/alerts")
async def export_alerts_csv(current_user=Depends(get_current_user)):
    """Export all alerts as CSV for reporting."""
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("""
            SELECT a.id, a.severity, a.message, a.is_acknowledged,
                   a.created_at, d.hostname, d.device_name, d.vendor_id
            FROM alerts a
            LEFT JOIN device_logs d ON a.device_log_id = d.id
            ORDER BY a.created_at DESC
        """)
        rows = [dict(r) for r in await cur.fetchall()]

    lines = ["ID,Severity,Hostname,Device Name,VID,Acknowledged,Timestamp,Message"]
    for r in rows:
        msg = str(r['message']).replace(',', ';')
        lines.append(
            f"{r['id']},{r['severity']},{r['hostname']},{r['device_name']},"
            f"{r['vendor_id']},{'Yes' if r['is_acknowledged'] else 'No'},"
            f"{r['created_at']},{msg}"
        )
    csv_content = "\n".join(lines)

    return Response(
        content=csv_content,
        media_type="text/csv",
        headers={"Content-Disposition": "attachment; filename=edtmrs_alerts.csv"}
    )




# ─── ACTION RESULT (agent reports back after blocking) ───────────────────────

@router.post("/action-result")
async def action_result(request: Request):
    """Agent reports result of physical block/allow action."""
    body = await request.json()
    log_id = body.get("log_id")
    action_result = body.get("action_result", "")
    hostname = body.get("hostname", "")

    logger.info(f"Action result from {hostname}: {action_result} for log_id={log_id}")

    if log_id and action_result == "blocked":
        async with aiosqlite.connect(DB_PATH) as db:
            await db.execute(
                "UPDATE device_logs SET status = 'physically_blocked' WHERE id = ?",
                (log_id,)
            )
            await db.commit()

        # Broadcast to dashboard
        await manager.broadcast({
            "type": "device_blocked",
            "data": {
                "log_id": log_id,
                "hostname": hostname,
                "message": f"Device physically blocked on {hostname}"
            }
        })

    return {"status": "ok"}



# ─── CLEANUP: Remove duplicate device records ────────────────────────────────

@router.post("/admin/cleanup-duplicates")
async def cleanup_duplicates(current_user=Depends(require_admin)):
    """Remove duplicate device_logs entries — keep only the most recent
    record for each unique device (by vendor_id+product_id+hostname).
    Run this once after upgrading to fix existing duplicate data."""
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row

        # Find all duplicates and keep only the row with the highest id
        await db.execute("""
            DELETE FROM device_logs
            WHERE id NOT IN (
                SELECT MAX(id)
                FROM device_logs
                GROUP BY
                    COALESCE(NULLIF(vendor_id,'unknown'), device_name),
                    COALESCE(NULLIF(product_id,'unknown'), device_name),
                    hostname
            )
        """)
        removed = db.total_changes
        await db.commit()

    return {"status": "ok", "removed": removed,
            "message": f"Removed {removed} duplicate records"}


@router.get("/admin/cleanup-duplicates")
async def cleanup_duplicates_get(current_user=Depends(require_admin)):
    """Same as POST — convenient for browser testing."""
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        await db.execute("""
            DELETE FROM device_logs
            WHERE id NOT IN (
                SELECT MAX(id)
                FROM device_logs
                GROUP BY
                    COALESCE(NULLIF(vendor_id,'unknown'), device_name),
                    COALESCE(NULLIF(product_id,'unknown'), device_name),
                    hostname
            )
        """)
        removed = db.total_changes
        await db.commit()
    return {"status": "ok", "removed": removed,
            "message": f"Removed {removed} duplicate records"}


# ─── HEARTBEAT (agent keepalive) ──────────────────────────────────────────────

@router.post("/heartbeat")
async def heartbeat(request: Request):
    """Agent heartbeat to keep endpoint status online."""
    body = await request.json()
    hostname = body.get("hostname", "")
    if hostname:
        async with aiosqlite.connect(DB_PATH) as db:
            await db.execute("""
                UPDATE endpoints SET status='online', last_seen=CURRENT_TIMESTAMP
                WHERE hostname=?
            """, (hostname,))
            await db.commit()
    return {"status": "ok"}
