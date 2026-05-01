import aiosqlite, logging
from fastapi import APIRouter, Depends, HTTPException, Response
from database import DB_PATH
from auth import get_current_user, require_admin, verify_password, create_token
from models import (LoginRequest, DeviceEventPayload, HeartbeatPayload,
                    BlockDeviceRequest, WhitelistDeviceRequest,
                    IsolateRequest, AcknowledgeRequest, ActionResultPayload)
from websocket_manager import manager

logger = logging.getLogger(__name__)
router = APIRouter()

# ── RISK CLASSIFIER ───────────────────────────────────────────────────────────
async def classify(db, payload: DeviceEventPayload) -> str:
    # 1. Whitelisted → SAFE
    cur = await db.execute(
        "SELECT id FROM device_whitelist WHERE vendor_id=? AND product_id=?",
        (payload.vendor_id, payload.product_id))
    if await cur.fetchone():
        return "safe"
    # 2. Blocked → CRITICAL
    cur = await db.execute(
        "SELECT id FROM blocked_devices WHERE vendor_id=? AND product_id=?",
        (payload.vendor_id, payload.product_id))
    if await cur.fetchone():
        return "critical"
    # 3. Malicious files on drive → CRITICAL or HIGH
    if payload.dangerous_files:
        low = [f.lower() for f in payload.dangerous_files]
        if "autorun.inf" in low:
            return "critical"
        malware_ext = {'.exe','.bat','.cmd','.scr','.vbs','.vbe','.js','.ps1','.hta','.pif','.com'}
        n = sum(1 for f in low if any(f.endswith(e) for e in malware_ext))
        if n >= 2: return "critical"
        if n == 1: return "high"
    # 4. Unknown identifiers → HIGH
    if payload.serial_number in ("unknown", "", None):
        return "high"
    if payload.vendor_id in ("unknown", "", None):
        return "high"
    # 5. Normal new pendrive → MEDIUM (not a threat, just unseen)
    return "medium"

# ── AUTH ──────────────────────────────────────────────────────────────────────
@router.post("/api/auth/login")
async def login(req: LoginRequest):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM users WHERE username=?", (req.username,))
        user = await cur.fetchone()
    if not user or not verify_password(req.password, user["password_hash"]):
        raise HTTPException(status_code=401, detail="Invalid credentials")
    token = create_token({"username": req.username, "role": user["role"]})
    return {"access_token": token, "token_type": "bearer",
            "user": {"username": req.username, "role": user["role"]}}

@router.get("/api/auth/me")
async def me(user=Depends(get_current_user)):
    return user

# ── DEVICE EVENT ──────────────────────────────────────────────────────────────
@router.post("/api/device-event")
async def device_event(payload: DeviceEventPayload):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row

        # Upsert endpoint record
        cur = await db.execute("SELECT id FROM endpoints WHERE hostname=?", (payload.hostname,))
        ep  = await cur.fetchone()
        if ep:
            endpoint_id = ep["id"]
            await db.execute(
                "UPDATE endpoints SET status='online',last_seen=CURRENT_TIMESTAMP,username=? WHERE id=?",
                (payload.username, endpoint_id))
        else:
            cur = await db.execute(
                "INSERT INTO endpoints (hostname,username,agent_version,status) VALUES (?,?,?,'online')",
                (payload.hostname, payload.username, payload.agent_version))
            endpoint_id = cur.lastrowid
        await db.commit()

        # Classify risk
        risk = await classify(db, payload)
        dangerous_str = ",".join(payload.dangerous_files)

        # Check if device seen before on this endpoint
        cur = await db.execute(
            "SELECT id FROM device_logs WHERE vendor_id=? AND product_id=? AND hostname=? LIMIT 1",
            (payload.vendor_id, payload.product_id, payload.hostname))
        existing = await cur.fetchone()
        if not existing:
            cur = await db.execute(
                "SELECT id FROM device_logs WHERE device_name=? AND hostname=? LIMIT 1",
                (payload.device_name, payload.hostname))
            existing = await cur.fetchone()

        if existing:
            log_id = existing["id"]
            await db.execute(
                """UPDATE device_logs SET last_seen=CURRENT_TIMESTAMP,
                   connect_count=connect_count+1, drive_letter=?, username=?,
                   risk_level=?, status='detected', dangerous_files=?, file_count=?
                   WHERE id=?""",
                (payload.drive_letter, payload.username, risk,
                 dangerous_str, payload.file_count, log_id))
            is_new = False
        else:
            cur = await db.execute(
                """INSERT INTO device_logs
                   (endpoint_id,vendor_id,product_id,serial_number,device_name,device_type,
                    drive_letter,hostname,username,risk_level,status,connect_count,
                    dangerous_files,file_count)
                   VALUES (?,?,?,?,?,?,?,?,?,?,'detected',1,?,?)""",
                (endpoint_id, payload.vendor_id, payload.product_id,
                 payload.serial_number, payload.device_name, payload.device_type,
                 payload.drive_letter, payload.hostname, payload.username,
                 risk, dangerous_str, payload.file_count))
            log_id = cur.lastrowid
            is_new = True
        await db.commit()

        # Create alert
        if (is_new and risk in ("medium","high","critical")) or \
           (not is_new and risk == "critical"):
            extra = ""
            if payload.dangerous_files:
                extra = f" | MALICIOUS: {', '.join(payload.dangerous_files[:3])}"
            msg = f"{risk.upper()} risk on {payload.hostname}: {payload.device_name}{extra}"
            await db.execute(
                """INSERT INTO alerts
                   (device_log_id,severity,message,hostname,username,device_name,drive_letter)
                   VALUES (?,?,?,?,?,?,?)""",
                (log_id, risk, msg, payload.hostname, payload.username,
                 payload.device_name, payload.drive_letter))
            await db.commit()

        # Broadcast to dashboard
        evt = {
            "log_id": log_id, "hostname": payload.hostname,
            "device_name": payload.device_name, "vendor_id": payload.vendor_id,
            "product_id": payload.product_id, "risk_level": risk,
            "drive_letter": payload.drive_letter, "username": payload.username,
            "timestamp": payload.timestamp, "dangerous_files": payload.dangerous_files,
        }
        await manager.send_device_event(evt)
        if risk in ("medium","high","critical"):
            await manager.send_alert(evt)

        # Tell agent what to do
        action = "none"
        if risk == "critical": action = "block"
        elif risk == "safe":   action = "allow"

        logger.info(f"Device event: {payload.device_name} [{risk}] from {payload.hostname}")
        return {"status":"ok","log_id":log_id,"risk_level":risk,"action":action}

# ── ACTION RESULT ─────────────────────────────────────────────────────────────
@router.post("/api/action-result")
async def action_result(payload: ActionResultPayload):
    if payload.log_id and payload.action_result == "blocked":
        async with aiosqlite.connect(DB_PATH) as db:
            await db.execute(
                "UPDATE device_logs SET status='physically_blocked' WHERE id=?",
                (payload.log_id,))
            await db.commit()
        await manager.broadcast({"type":"device_blocked",
            "data":{"log_id":payload.log_id,"hostname":payload.hostname}})
    return {"status":"ok"}

# ── PENDING ACTIONS (agent polls every 5s) ────────────────────────────────────
@router.get("/api/pending-actions/{hostname}")
async def pending_actions(hostname: str):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute(
            "SELECT * FROM pending_actions WHERE hostname=? AND executed=0 ORDER BY created_at",
            (hostname,))
        actions = [dict(r) for r in await cur.fetchall()]
        if actions:
            ids = [a["id"] for a in actions]
            await db.execute(
                "UPDATE pending_actions SET executed=1 WHERE id IN ({})".format(
                    ",".join("?"*len(ids))), ids)
            await db.commit()
    return {"actions": actions}

# ── BLOCK DEVICE ──────────────────────────────────────────────────────────────
@router.post("/api/block-device")
async def block_device(payload: BlockDeviceRequest, user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        # Remove from whitelist if there
        await db.execute(
            "DELETE FROM device_whitelist WHERE vendor_id=? AND product_id=?",
            (payload.vendor_id, payload.product_id))
        # Add to blocked
        await db.execute(
            """INSERT OR IGNORE INTO blocked_devices
               (vendor_id,product_id,serial_number,device_name,blocked_by,reason)
               VALUES (?,?,?,?,?,?)""",
            (payload.vendor_id, payload.product_id, payload.serial_number,
             payload.device_name, user["username"], payload.reason))
        # Queue immediate block for active endpoints
        cur = await db.execute(
            """SELECT DISTINCT hostname FROM device_logs
               WHERE vendor_id=? AND product_id=?
                 AND last_seen >= DATETIME('now','-10 minutes')""",
            (payload.vendor_id, payload.product_id))
        hosts = [r["hostname"] for r in await cur.fetchall()]
        for h in hosts:
            await db.execute(
                """INSERT INTO pending_actions
                   (hostname,action,vendor_id,product_id,serial_number,device_name,executed)
                   VALUES (?,'block',?,?,?,?,0)""",
                (h, payload.vendor_id, payload.product_id,
                 payload.serial_number, payload.device_name))
        await db.commit()
    await manager.broadcast({"type":"device_blocked_policy",
        "data":{"device_name":payload.device_name,"vendor_id":payload.vendor_id}})
    return {"status":"ok","message":f"Blocked. Action queued for {len(hosts)} endpoint(s)."}

# ── WHITELIST DEVICE ──────────────────────────────────────────────────────────
@router.post("/api/whitelist-device")
async def whitelist_device(payload: WhitelistDeviceRequest, user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        # Remove from blocked
        await db.execute(
            "DELETE FROM blocked_devices WHERE vendor_id=? AND product_id=?",
            (payload.vendor_id, payload.product_id))
        # Add to whitelist
        await db.execute(
            """INSERT OR IGNORE INTO device_whitelist
               (vendor_id,product_id,serial_number,device_name,added_by,notes)
               VALUES (?,?,?,?,?,?)""",
            (payload.vendor_id, payload.product_id, payload.serial_number,
             payload.device_name, user["username"], payload.notes))
        # Queue immediate unblock for active endpoints
        cur = await db.execute(
            """SELECT DISTINCT hostname FROM device_logs
               WHERE vendor_id=? AND product_id=?
                 AND last_seen >= DATETIME('now','-10 minutes')""",
            (payload.vendor_id, payload.product_id))
        hosts = [r["hostname"] for r in await cur.fetchall()]
        for h in hosts:
            await db.execute(
                """INSERT INTO pending_actions
                   (hostname,action,vendor_id,product_id,serial_number,device_name,executed)
                   VALUES (?,'allow',?,?,?,?,0)""",
                (h, payload.vendor_id, payload.product_id,
                 payload.serial_number, payload.device_name))
        await db.commit()
    return {"status":"ok","message":f"Whitelisted. Unblock queued for {len(hosts)} endpoint(s)."}

# ── STATS ─────────────────────────────────────────────────────────────────────
@router.get("/api/stats")
async def stats(user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        async def cnt(sql): return (await (await db.execute(sql)).fetchone())[0]
        r = {
            "total_endpoints":       await cnt("SELECT COUNT(*) FROM endpoints"),
            "online_endpoints":      await cnt("SELECT COUNT(*) FROM endpoints WHERE status='online'"),
            "total_devices":         await cnt("SELECT COUNT(*) FROM device_logs"),
            "suspicious_devices":    await cnt("SELECT COUNT(*) FROM device_logs WHERE risk_level IN ('high','critical')"),
            "total_alerts":          await cnt("SELECT COUNT(*) FROM alerts"),
            "unacknowledged_alerts": await cnt("SELECT COUNT(*) FROM alerts WHERE is_acknowledged=0"),
        }
        cur = await db.execute(
            """SELECT DATE(timestamp) day, COUNT(*) count FROM device_logs
               WHERE timestamp>=DATE('now','-7 days')
               GROUP BY DATE(timestamp) ORDER BY day""")
        r["activity_7d"] = [dict(row) for row in await cur.fetchall()]
    return r

# ── DEVICES ───────────────────────────────────────────────────────────────────
@router.get("/api/devices")
async def get_devices(search:str=None, risk_level:str=None,
                      limit:int=100, user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        q = "SELECT * FROM device_logs WHERE 1=1"
        p = []
        if search:
            q += " AND (device_name LIKE ? OR hostname LIKE ? OR vendor_id LIKE ? OR serial_number LIKE ?)"
            p += [f"%{search}%"]*4
        if risk_level:
            q += " AND risk_level=?"
            p.append(risk_level)
        q += " ORDER BY last_seen DESC LIMIT ?"
        p.append(limit)
        cur  = await db.execute(q, p)
        devs = [dict(r) for r in await cur.fetchall()]
        tot  = (await (await db.execute("SELECT COUNT(*) FROM device_logs")).fetchone())[0]
    return {"devices": devs, "total": tot}

# ── ENDPOINTS ─────────────────────────────────────────────────────────────────
@router.get("/api/endpoints")
async def get_endpoints(user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM endpoints ORDER BY last_seen DESC")
        return {"endpoints": [dict(r) for r in await cur.fetchall()]}

@router.post("/api/isolate-endpoint")
async def isolate(payload: IsolateRequest, user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        await db.execute(
            "UPDATE endpoints SET is_isolated=1,isolation_reason=? WHERE id=?",
            (payload.reason, payload.endpoint_id))
        await db.commit()
        cur = await db.execute("SELECT hostname FROM endpoints WHERE id=?", (payload.endpoint_id,))
        row = await cur.fetchone()
        hostname = row["hostname"] if row else ""
    await manager.broadcast({"type":"endpoint_isolated","data":{"hostname":hostname}})
    return {"status":"ok"}

@router.post("/api/unisolate-endpoint/{eid}")
async def unisolate(eid:int, user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("UPDATE endpoints SET is_isolated=0,isolation_reason='' WHERE id=?", (eid,))
        await db.commit()
    return {"status":"ok"}

# ── ALERTS ────────────────────────────────────────────────────────────────────
@router.get("/api/alerts")
async def get_alerts(acknowledged:bool=None, limit:int=100, user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        q = "SELECT * FROM alerts WHERE 1=1"
        p = []
        if acknowledged is not None:
            q += " AND is_acknowledged=?"
            p.append(1 if acknowledged else 0)
        q += " ORDER BY created_at DESC LIMIT ?"
        p.append(limit)
        cur = await db.execute(q, p)
        return {"alerts": [dict(r) for r in await cur.fetchall()]}

@router.post("/api/alerts/acknowledge")
async def ack_alert(payload: AcknowledgeRequest, user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("UPDATE alerts SET is_acknowledged=1 WHERE id=?", (payload.alert_id,))
        await db.commit()
    return {"status":"ok"}

@router.post("/api/alerts/acknowledge-all")
async def ack_all(user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("UPDATE alerts SET is_acknowledged=1")
        await db.commit()
    return {"status":"ok"}

# ── WHITELIST / BLOCKED LISTS ─────────────────────────────────────────────────
@router.get("/api/whitelist")
async def get_whitelist(user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM device_whitelist ORDER BY created_at DESC")
        return {"whitelist": [dict(r) for r in await cur.fetchall()]}

@router.delete("/api/whitelist/{wid}")
async def del_whitelist(wid:int, user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute("DELETE FROM device_whitelist WHERE id=?", (wid,))
        await db.commit()
    return {"status":"ok"}

@router.get("/api/blocked-devices")
async def get_blocked(user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM blocked_devices ORDER BY created_at DESC")
        return {"blocked_devices": [dict(r) for r in await cur.fetchall()]}

@router.delete("/api/blocked-devices/{bid}")
async def del_blocked(bid:int, user=Depends(require_admin)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT * FROM blocked_devices WHERE id=?", (bid,))
        dev = await cur.fetchone()
        await db.execute("DELETE FROM blocked_devices WHERE id=?", (bid,))
        if dev:
            cur = await db.execute(
                """SELECT DISTINCT hostname FROM device_logs
                   WHERE vendor_id=? AND product_id=?
                     AND last_seen>=DATETIME('now','-10 minutes')""",
                (dev["vendor_id"], dev["product_id"]))
            for r in await cur.fetchall():
                await db.execute(
                    """INSERT INTO pending_actions
                       (hostname,action,vendor_id,product_id,serial_number,device_name,executed)
                       VALUES (?,'allow',?,?,?,?,0)""",
                    (r["hostname"], dev["vendor_id"], dev["product_id"],
                     dev["serial_number"], dev["device_name"]))
        await db.commit()
    return {"status":"ok"}

# ── CSV EXPORT ────────────────────────────────────────────────────────────────
@router.get("/api/export/devices")
async def export_devices(user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur  = await db.execute("SELECT * FROM device_logs ORDER BY timestamp DESC")
        rows = [dict(r) for r in await cur.fetchall()]
    lines = ["ID,Device Name,VID,PID,Serial,Drive,Hostname,Username,Risk,Status,Connections,Last Seen"]
    for r in rows:
        lines.append(",".join(str(r.get(k,"")) for k in
            ["id","device_name","vendor_id","product_id","serial_number",
             "drive_letter","hostname","username","risk_level","status","connect_count","last_seen"]))
    return Response(content="\n".join(lines), media_type="text/csv",
        headers={"Content-Disposition":"attachment; filename=edtmrs_devices.csv"})

@router.get("/api/export/alerts")
async def export_alerts(user=Depends(get_current_user)):
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur  = await db.execute("SELECT * FROM alerts ORDER BY created_at DESC")
        rows = [dict(r) for r in await cur.fetchall()]
    lines = ["ID,Severity,Hostname,Device,Acknowledged,Timestamp,Message"]
    for r in rows:
        lines.append(
            f"{r['id']},{r['severity']},{r['hostname']},{r['device_name']},"
            f"{'Yes' if r['is_acknowledged'] else 'No'},{r['created_at']},"
            f"{str(r['message']).replace(',',';')}")
    return Response(content="\n".join(lines), media_type="text/csv",
        headers={"Content-Disposition":"attachment; filename=edtmrs_alerts.csv"})

# ── HEARTBEAT ─────────────────────────────────────────────────────────────────
@router.post("/api/heartbeat")
async def heartbeat(payload: HeartbeatPayload):
    async with aiosqlite.connect(DB_PATH) as db:
        await db.execute(
            """INSERT INTO endpoints (hostname,ip_address,username,agent_version,status)
               VALUES (?,?,?,?,'online')
               ON CONFLICT(hostname) DO UPDATE SET
               status='online', last_seen=CURRENT_TIMESTAMP,
               ip_address=excluded.ip_address,
               username=excluded.username""",
            (payload.hostname, payload.ip_address, payload.username, payload.agent_version))
        await db.commit()
    return {"status":"ok"}

@router.get("/health")
async def health():
    return {"status":"ok","version":"6.1.0"}
