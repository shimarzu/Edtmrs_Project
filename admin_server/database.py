"""
EDTMRS - Database Configuration and Models
Uses SQLite via aiosqlite for async support
"""

import aiosqlite
import bcrypt
import logging
import os

logger = logging.getLogger(__name__)

DB_PATH = os.environ.get("EDTMRS_DB", "edtmrs.db")

SCHEMA = """
CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    username TEXT UNIQUE NOT NULL,
    email TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    role TEXT NOT NULL DEFAULT 'admin',
    is_active INTEGER NOT NULL DEFAULT 1,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS endpoints (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    hostname TEXT NOT NULL UNIQUE,
    ip_address TEXT,
    username TEXT,
    os_info TEXT,
    agent_version TEXT DEFAULT '1.0.0',
    status TEXT DEFAULT 'offline',
    last_seen DATETIME,
    is_isolated INTEGER DEFAULT 0,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS device_logs (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    endpoint_id INTEGER,
    vendor_id TEXT,
    product_id TEXT,
    serial_number TEXT,
    device_name TEXT,
    device_type TEXT,
    drive_letter TEXT,
    hostname TEXT,
    username TEXT,
    timestamp DATETIME DEFAULT CURRENT_TIMESTAMP,
    last_seen DATETIME DEFAULT CURRENT_TIMESTAMP,
    connect_count INTEGER DEFAULT 1,
    risk_level TEXT DEFAULT 'unknown',
    status TEXT DEFAULT 'detected',
    raw_data TEXT,
    FOREIGN KEY (endpoint_id) REFERENCES endpoints(id)
);

CREATE TABLE IF NOT EXISTS alerts (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    device_log_id INTEGER,
    endpoint_id INTEGER,
    alert_type TEXT NOT NULL,
    severity TEXT DEFAULT 'medium',
    message TEXT,
    is_acknowledged INTEGER DEFAULT 0,
    acknowledged_by INTEGER,
    acknowledged_at DATETIME,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (device_log_id) REFERENCES device_logs(id),
    FOREIGN KEY (endpoint_id) REFERENCES endpoints(id)
);

CREATE TABLE IF NOT EXISTS device_whitelist (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vendor_id TEXT,
    product_id TEXT,
    serial_number TEXT,
    device_name TEXT,
    added_by INTEGER,
    notes TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

CREATE TABLE IF NOT EXISTS blocked_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vendor_id TEXT,
    product_id TEXT,
    serial_number TEXT,
    device_name TEXT,
    blocked_by INTEGER,
    reason TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
);
"""


async def get_db():
    """Get async database connection."""
    db = await aiosqlite.connect(DB_PATH)
    db.row_factory = aiosqlite.Row
    try:
        yield db
    finally:
        await db.close()


async def init_db():
    """Initialize the database with schema."""
    async with aiosqlite.connect(DB_PATH) as db:
        await db.executescript(SCHEMA)

        # Migration: add new columns if upgrading from older version
        migrations = [
            "ALTER TABLE device_logs ADD COLUMN connect_count INTEGER DEFAULT 1",
            "ALTER TABLE device_logs ADD COLUMN last_seen DATETIME DEFAULT CURRENT_TIMESTAMP",
        ]
        for migration in migrations:
            try:
                await db.execute(migration)
                await db.commit()
            except Exception:
                pass  # Column already exists — that's fine

    logger.info(f"Database initialized at {DB_PATH}")


async def create_default_admin():
    """Create default admin account if not exists."""
    async with aiosqlite.connect(DB_PATH) as db:
        db.row_factory = aiosqlite.Row
        cursor = await db.execute(
            "SELECT id FROM users WHERE username = ?", ("admin",)
        )
        row = await cursor.fetchone()
        if not row:
            password = "Admin@1234"
            hashed = bcrypt.hashpw(password.encode(), bcrypt.gensalt()).decode()
            await db.execute(
                """INSERT INTO users (username, email, password_hash, role)
                   VALUES (?, ?, ?, ?)""",
                ("admin", "admin@edtmrs.local", hashed, "superadmin")
            )
            await db.commit()
            logger.info("Default admin account created: admin / Admin@1234")
