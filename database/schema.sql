-- EDTMRS Database Schema
-- Run this to initialize the SQLite / PostgreSQL database

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
    hostname TEXT NOT NULL,
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
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (added_by) REFERENCES users(id)
);

CREATE TABLE IF NOT EXISTS blocked_devices (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    vendor_id TEXT,
    product_id TEXT,
    serial_number TEXT,
    device_name TEXT,
    blocked_by INTEGER,
    reason TEXT,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    FOREIGN KEY (blocked_by) REFERENCES users(id)
);

-- Default admin user (password: Admin@1234)
-- Hash generated with bcrypt
INSERT OR IGNORE INTO users (username, email, password_hash, role)
VALUES (
    'admin',
    'admin@edtmrs.local',
    '$2b$12$LQv3c1yqBWVHxkd0LHAkCOYz6TtxMlJbekRTbL4EeKSqPVhSjlCHu',
    'superadmin'
);
