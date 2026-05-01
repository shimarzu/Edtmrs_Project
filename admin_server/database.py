import aiosqlite, logging, os
logger  = logging.getLogger(__name__)
DB_PATH = os.path.join(os.path.dirname(__file__), "edtmrs.db")

SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    email TEXT DEFAULT '',
    role TEXT DEFAULT 'admin',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS endpoints (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    hostname TEXT UNIQUE NOT NULL,
    ip_address TEXT DEFAULT '',
    username TEXT DEFAULT '',
    agent_version TEXT DEFAULT 'unknown',
    status TEXT DEFAULT 'online',
    is_isolated INTEGER DEFAULT 0,
    isolation_reason TEXT DEFAULT '',
    last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
    first_seen DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS device_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    endpoint_id INTEGER,
    vendor_id TEXT DEFAULT 'unknown',
    product_id TEXT DEFAULT 'unknown',
    serial_number TEXT DEFAULT 'unknown',
    device_name TEXT DEFAULT 'Unknown Device',
    device_type TEXT DEFAULT 'USB Storage',
    drive_letter TEXT DEFAULT '',
    hostname TEXT DEFAULT '',
    username TEXT DEFAULT '',
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
    connect_count INTEGER DEFAULT 1,
    risk_level TEXT DEFAULT 'medium',
    status TEXT DEFAULT 'detected',
    dangerous_files TEXT DEFAULT '',
    file_count INTEGER DEFAULT 0
);
CREATE TABLE IF NOT EXISTS alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_log_id INTEGER,
    severity TEXT DEFAULT 'medium',
    message TEXT DEFAULT '',
    hostname TEXT DEFAULT '',
    username TEXT DEFAULT '',
    device_name TEXT DEFAULT '',
    drive_letter TEXT DEFAULT '',
    is_acknowledged INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS device_whitelist (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vendor_id TEXT DEFAULT '',
    product_id TEXT DEFAULT '',
    serial_number TEXT DEFAULT 'unknown',
    device_name TEXT DEFAULT '',
    added_by TEXT DEFAULT 'admin',
    notes TEXT DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS blocked_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vendor_id TEXT DEFAULT '',
    product_id TEXT DEFAULT '',
    serial_number TEXT DEFAULT 'unknown',
    device_name TEXT DEFAULT '',
    blocked_by TEXT DEFAULT 'admin',
    reason TEXT DEFAULT '',
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE IF NOT EXISTS pending_actions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    hostname TEXT NOT NULL,
    action TEXT NOT NULL,
    vendor_id TEXT DEFAULT '',
    product_id TEXT DEFAULT '',
    serial_number TEXT DEFAULT 'unknown',
    device_name TEXT DEFAULT '',
    executed INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
"""

async def init_db():
    async with aiosqlite.connect(DB_PATH) as db:
        await db.executescript(SCHEMA)
        for m in [
            "ALTER TABLE device_logs ADD COLUMN connect_count INTEGER DEFAULT 1",
            "ALTER TABLE device_logs ADD COLUMN last_seen DATETIME DEFAULT CURRENT_TIMESTAMP",
            "ALTER TABLE device_logs ADD COLUMN dangerous_files TEXT DEFAULT ''",
            "ALTER TABLE device_logs ADD COLUMN file_count INTEGER DEFAULT 0",
        ]:
            try: await db.execute(m); await db.commit()
            except: pass
    logger.info(f"DB ready: {DB_PATH}")

async def create_default_admin():
    from auth import hash_password
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cur = await db.execute("SELECT id FROM users WHERE username='admin'")
        if not await cur.fetchone():
            await db.execute(
                "INSERT INTO users (username,password_hash,email,role) VALUES (?,?,?,?)",
                ("admin", hash_password("Admin@1234"), "admin@edtmrs.local", "admin"))
            await db.commit()
            logger.info("Default admin created: admin / Admin@1234")
