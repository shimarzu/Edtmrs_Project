# EDTMRS — External Device Threat Monitoring and Response System

![Version](https://img.shields.io/badge/version-6.1.0-blue)
![Python](https://img.shields.io/badge/python-3.11+-green)
![React](https://img.shields.io/badge/react-18-blue)
![Platform](https://img.shields.io/badge/agent-Windows%2010%2F11-lightgrey)
![License](https://img.shields.io/badge/license-MIT-green)

> **SLIIT Information Security Project (IE3092)**
> Shimar Z.A.M — IT23215092 | Kavindya R.M.D — IT23429246
> Supervisor: Mr. Tharaniyawarma Kumaralingam

---

## What is EDTMRS?

EDTMRS is a lightweight, real-time USB endpoint security platform. It monitors every USB storage device inserted into monitored workstations across an organization, classifies the threat level, sends instant alerts to a web dashboard, and allows administrators to physically block or whitelist devices — all from a single admin panel.

---

## How It Works

```
User PC 1 ──┐
User PC 2 ──┼──→ Admin Server (FastAPI) ──→ React Dashboard
User PC N ──┘         ↑                          ↑
                 All USB events            Real-time alerts
                 reported here             Block / Whitelist
```

1. USB inserted on any User PC
2. C++ Agent detects it within 1 second, scans files for malware
3. Event sent to Admin Server via HTTP POST
4. Server classifies risk: **SAFE / MEDIUM / HIGH / CRITICAL**
5. Dashboard receives WebSocket notification — popup appears instantly
6. Admin clicks **BLOCK** → USB physically disabled on User PC within 5 seconds
7. Admin clicks **WHITELIST** → USB re-enabled and marked as trusted

---

## Risk Levels

| Level | Meaning |
|-------|---------|
| 🟢 SAFE | Admin whitelisted — no alert |
| 🟡 MEDIUM | Normal new USB — review recommended |
| 🟠 HIGH | Unknown/unidentifiable device |
| 🔴 CRITICAL | Blocked by admin OR malicious files found (autorun.inf, .exe, .bat etc) |

---

## Project Structure

```
edtmrs_v2/
├── admin_server/          ← Python FastAPI backend
│   ├── main.py            ← Server entry point
│   ├── api_routes.py      ← All REST API endpoints
│   ├── auth.py            ← JWT authentication
│   ├── database.py        ← SQLite schema + migrations
│   ├── models.py          ← Pydantic request/response models
│   ├── websocket_manager.py ← Real-time WebSocket broadcaster
│   ├── test_server.py     ← Diagnostic test script
│   ├── requirements.txt
│   ├── START_ADMIN_SERVER.bat
│   └── OPEN_FIREWALL.bat
├── endpoint_agent/        ← C++ Windows agent
│   ├── main.cpp           ← Agent entry point + Windows service
│   ├── device_monitor.cpp ← USB drive polling (1s interval)
│   ├── device_monitor.h
│   ├── http_client.cpp    ← WinHTTP POST + GET
│   ├── http_client.h
│   ├── blocker.cpp        ← PowerShell USB block/unblock
│   ├── blocker.h
│   ├── config.ini         ← SERVER_HOST configuration
│   ├── compile_and_install.bat
│   ├── SETUP_USER_PC.bat  ← One-click user PC setup
│   └── UNINSTALL.bat
└── dashboard/             ← React 18 web dashboard
    ├── src/
    │   ├── App.js
    │   ├── pages/         ← Login, Dashboard, Devices, Endpoints, Alerts, Policy
    │   ├── components/    ← Sidebar, AlertPopup, RiskBadge
    │   ├── hooks/         ← useAuth, useWebSocket
    │   └── utils/api.js   ← All API calls
    ├── .env               ← REACT_APP_API_URL config
    ├── package.json
    └── START_DASHBOARD.bat
```

---

## Quick Start

### Admin PC

```powershell
# 1. Open firewall (run once as Admin)
OPEN_FIREWALL.bat

# 2. Install dependencies
cd admin_server
pip install -r requirements.txt

# 3. Start server
python main.py

# 4. Start dashboard (new terminal)
cd dashboard
npm install
npm start
```

Open browser → `http://localhost:3000` → Login: `admin` / `Admin@1234`

### User PC (repeat for each PC)

```powershell
# 1. Copy endpoint_agent folder to User PC

# 2. Edit config.ini - set Admin PC IP
SERVER_HOST=192.168.1.10   ← your Admin PC IP

# 3. Compile agent (requires w64devkit)
g++ -std=c++17 -O2 -o edtmrs_agent.exe ^
    main.cpp device_monitor.cpp http_client.cpp blocker.cpp ^
    -lwinhttp -lsetupapi -lcfgmgr32 -lws2_32

# 4. Install as service (right-click → Run as Administrator)
SETUP_USER_PC.bat
```

---

## Testing

### Verify server works
```powershell
cd admin_server
python test_server.py
```
All 9 tests should show ✅ PASS.

### Test malicious file detection (safe — empty files)
```powershell
echo test > D:\autorun.inf
echo test > D:\virus.exe
echo test > D:\run.bat
```
Eject and reinsert → dashboard shows 🔴 CRITICAL

---

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/auth/login` | Admin login → returns JWT token |
| POST | `/api/device-event` | Agent reports USB insertion |
| POST | `/api/heartbeat` | Agent keepalive |
| GET | `/api/pending-actions/{hostname}` | Agent polls for block commands |
| GET | `/api/devices` | List all device records |
| GET | `/api/endpoints` | List all monitored endpoints |
| GET | `/api/alerts` | List all threat alerts |
| POST | `/api/block-device` | Block a device (queues command to agent) |
| POST | `/api/whitelist-device` | Whitelist a device |
| POST | `/api/alerts/acknowledge` | Acknowledge an alert |
| GET | `/api/export/devices` | Export device logs as CSV |
| GET | `/api/export/alerts` | Export alerts as CSV |
| WS | `/ws/alerts` | WebSocket real-time events |

---

## Tech Stack

| Component | Technology |
|-----------|-----------|
| Endpoint Agent | C++17, WinHTTP, SetupAPI, PowerShell PnP |
| Backend | Python 3.11, FastAPI, SQLite (aiosqlite) |
| Authentication | JWT (PyJWT), bcrypt |
| Real-Time | WebSocket (FastAPI native) |
| Frontend | React 18, Chart.js, Axios |
| USB Blocking | PowerShell `Disable-PnpDevice` / `Enable-PnpDevice` |

---

## Requirements

**Admin PC:** Python 3.11+, Node.js 18+, any OS

**User PC:** Windows 10/11 (64-bit), w64devkit (MinGW) for compilation, Administrator privileges

---

## Default Credentials

```
Username: admin
Password: Admin@1234
```

Change after first login in production.

---

## Troubleshooting

**USB not showing on dashboard**
- Check agent log: `type C:\edtmrs\endpoint_agent\edtmrs_agent.log`
- Verify `SERVER_HOST` in `config.ini` matches Admin PC IP
- Run `OPEN_FIREWALL.bat` on Admin PC as Administrator

**Login fails**
- Ensure `python main.py` is running
- Delete `edtmrs.db` and restart server for fresh credentials

**Block not working**
- Agent must run as Administrator (service runs as SYSTEM ✅)
- Check agent log for `BLOCKED_OK`

**npm start fails**
```powershell
rmdir /s /q node_modules && del package-lock.json
npm install && npm start
```
> ⚠️ Never run `npm audit fix --force`

---

## License

MIT License — see LICENSE file for details.
