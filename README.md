# EDTMRS — External Device Threat Monitoring and Response System
### SLIIT Cybersecurity Project | Shimar Z.A.M. (IT23215092) & Kavindya R.M.D. (IT23429246)

---

## System Architecture

```
User PC (Endpoint Agent C++)
         │  HTTP POST /api/device-event
         ▼
Admin PC (FastAPI Server :8000)
         │
         ├── SQLite Database (edtmrs.db)
         │
         └── WebSocket /ws/alerts
                  │
                  ▼
         React Dashboard (:3000)  ← Real-time alert popups
```

---

## File Structure

```
edtmrs/
├── admin_server/
│   ├── main.py              ← FastAPI app entry point
│   ├── database.py          ← SQLite async DB + schema init
│   ├── auth.py              ← JWT + bcrypt authentication
│   ├── models.py            ← Pydantic request/response models
│   ├── api_routes.py        ← All REST API endpoints
│   ├── websocket_manager.py ← WebSocket real-time broadcasts
│   └── requirements.txt     ← Python dependencies
│
├── dashboard/
│   ├── public/index.html
│   ├── src/
│   │   ├── App.js           ← Root router + WebSocket listener
│   │   ├── index.js         ← React entry point
│   │   ├── index.css        ← Tailwind CSS
│   │   ├── hooks/
│   │   │   ├── useAuth.js   ← Auth context (login/logout/JWT)
│   │   │   └── useWebSocket.js ← Auto-reconnect WS hook
│   │   ├── utils/
│   │   │   └── api.js       ← Axios API client (all endpoints)
│   │   ├── components/
│   │   │   ├── Sidebar.jsx  ← Navigation sidebar
│   │   │   ├── RiskBadge.jsx ← Color-coded risk level badge
│   │   │   └── AlertPopup.jsx ← Real-time toast notifications
│   │   └── pages/
│   │       ├── LoginPage.jsx    ← Admin authentication
│   │       ├── DashboardPage.jsx ← Stats + charts + live feed
│   │       ├── DevicesPage.jsx  ← Device table + block/whitelist
│   │       ├── EndpointsPage.jsx ← User PCs + isolate action
│   │       ├── AlertsPage.jsx   ← Alert management
│   │       └── PolicyPage.jsx   ← Whitelist/blocklist manager
│   ├── package.json
│   └── tailwind.config.js
│
├── endpoint_agent/
│   ├── main.cpp             ← Entry point, service support
│   ├── device_monitor.cpp   ← WM_DEVICECHANGE + SetupAPI
│   ├── device_monitor.h
│   ├── http_client.cpp      ← WinHTTP POST requests
│   ├── http_client.h
│   ├── CMakeLists.txt       ← Build configuration
│   └── config.ini           ← SERVER_HOST, SERVER_PORT
│
├── database/
│   └── schema.sql           ← Reference SQL schema
│
└── scripts/
    ├── setup_admin.ps1      ← Admin PC automated setup
    ├── build_agent.ps1      ← User PC build helper
    └── start_server.py      ← Cross-platform server launcher
```

---

## ADMIN PC — Setup & Run

### Prerequisites
- Python 3.11+  →  https://python.org
- Node.js 18+   →  https://nodejs.org
- Both PCs on same LAN (or use one machine for testing)

### Step 1 — Install backend dependencies
```powershell
cd edtmrs\admin_server
pip install -r requirements.txt
```

### Step 2 — Start the FastAPI server
```powershell
python main.py
```
The server starts on `http://0.0.0.0:8000`.
The SQLite database `edtmrs.db` is auto-created on first run.
Default admin account is created: **admin / Admin@1234**

### Step 3 — Find your Admin PC's IP address
```powershell
ipconfig
# Look for IPv4 Address under your LAN adapter
# Example: 192.168.1.20
```

### Step 4 — Configure the React dashboard
```powershell
cd edtmrs\dashboard
```
Create a file `.env` with:
```
REACT_APP_API_URL=http://192.168.1.20:8000
```
Replace `192.168.1.20` with your actual Admin PC IP.

### Step 5 — Install and start the dashboard
```powershell
npm install
npm start
```
Dashboard opens at: **http://localhost:3000**

---

## USER PC — Build & Run Agent

### Prerequisites
Choose ONE compiler option:

**Option A — Visual Studio 2022 (recommended)**
1. Download: https://visualstudio.microsoft.com/
2. Install workload: **"Desktop development with C++"**
3. Download CMake: https://cmake.org/download/

**Option B — w64devkit (no Visual Studio needed)**
1. Download: https://github.com/skeeto/w64devkit/releases
2. Extract, add `bin\` to Windows PATH
3. No CMake needed — use manual compile below

---

### Step 1 — Edit config.ini
Open `endpoint_agent\config.ini` and set your Admin PC's IP:
```ini
SERVER_HOST=192.168.1.20
SERVER_PORT=8000
HEARTBEAT_INTERVAL=30
```

### Step 2 — Compile the agent

**With CMake (Option A):**
```powershell
cd edtmrs\endpoint_agent
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
# Output: build\Release\edtmrs_agent.exe
```

**With MinGW/w64devkit (Option B):**
```powershell
cd edtmrs\endpoint_agent
g++ -std=c++17 -O2 -o edtmrs_agent.exe main.cpp device_monitor.cpp http_client.cpp -lwinhttp -lsetupapi -lcfgmgr32 -lws2_32
```

### Step 3 — Run the agent

**A) Console mode (recommended for testing — you see output):**
```powershell
.\edtmrs_agent.exe
```

**B) Install as a Windows background service:**
```powershell
# Run as Administrator
.\edtmrs_agent.exe --install-service
net start EDTMRSAgent
```

**C) Remove the service:**
```powershell
net stop EDTMRSAgent
.\edtmrs_agent.exe --remove-service
```

---

## Two-Machine Network Test

```
User PC  : 192.168.1.10  (runs edtmrs_agent.exe)
Admin PC : 192.168.1.20  (runs FastAPI + React)
```

1. Start FastAPI on Admin PC → `python main.py`
2. Start React on Admin PC → `npm start`
3. Open browser: `http://localhost:3000` → login with admin/Admin@1234
4. On User PC, set `SERVER_HOST=192.168.1.20` in config.ini
5. Run `edtmrs_agent.exe` on User PC
6. **Insert a USB drive** into the User PC
7. Watch the Admin dashboard:
   - Popup alert appears in bottom-right
   - Device Monitor table updates
   - Alerts page shows new entry
   - Dashboard stats update

**Firewall note:** Allow port 8000 TCP on Admin PC:
```powershell
netsh advfirewall firewall add rule name="EDTMRS" dir=in action=allow protocol=TCP localport=8000
```

---

## API Reference

| Method | Endpoint                   | Description                        |
|--------|----------------------------|------------------------------------|
| POST   | /api/auth/login            | Login, get JWT token               |
| GET    | /api/auth/me               | Current user info                  |
| POST   | /api/device-event          | Receive event from agent (no auth) |
| POST   | /api/heartbeat             | Agent keepalive (no auth)          |
| GET    | /api/stats                 | Dashboard statistics               |
| GET    | /api/devices               | List all device logs               |
| GET    | /api/endpoints             | List all registered endpoints      |
| GET    | /api/alerts                | List alerts                        |
| POST   | /api/alerts/acknowledge    | Acknowledge single alert           |
| POST   | /api/alerts/acknowledge-all| Acknowledge all alerts             |
| POST   | /api/block-device          | Block a device by VID/PID/serial   |
| POST   | /api/whitelist-device      | Whitelist a device                 |
| GET    | /api/whitelist             | List whitelisted devices           |
| GET    | /api/blocked-devices       | List blocked devices               |
| DELETE | /api/whitelist/{id}        | Remove from whitelist              |
| DELETE | /api/blocked-devices/{id}  | Unblock a device                   |
| POST   | /api/isolate-endpoint      | Isolate an endpoint                |
| POST   | /api/unisolate-endpoint/{id}| Unisolate an endpoint             |
| WS     | /ws/alerts                 | WebSocket real-time event stream   |

Interactive API docs: **http://localhost:8000/docs**

---

## Risk Classification Logic

| Condition                              | Risk Level |
|----------------------------------------|------------|
| Device found in whitelist              | ✅ SAFE    |
| Device found in blocked list           | 🔴 CRITICAL|
| Serial number is "unknown" or empty    | 🟠 HIGH    |
| Vendor ID is "unknown" or empty        | 🟡 MEDIUM  |
| Known VID/PID/serial, not whitelisted  | 🟡 MEDIUM  |

---

## Database Tables

| Table            | Purpose                                        |
|------------------|------------------------------------------------|
| users            | Admin accounts with bcrypt hashed passwords   |
| endpoints        | Registered User PCs + online/offline status   |
| device_logs      | Every USB insertion event with metadata       |
| alerts           | Generated alerts with severity levels         |
| device_whitelist | Approved devices (no alerts generated)        |
| blocked_devices  | Banned devices (marked CRITICAL)              |

---

## Security Features

- **JWT Authentication** — all API endpoints protected (except device-event/heartbeat)
- **bcrypt password hashing** — industry-standard, cost factor 12
- **Role-based access** — superadmin, admin roles
- **Input validation** — Pydantic models on all endpoints
- **CORS** — configurable origins (currently open for development)

---

## Default Credentials
```
Username : admin
Password : Admin@1234
```
**Change this immediately in production!**

---

## Technology Stack

| Layer           | Technology                        |
|-----------------|-----------------------------------|
| Endpoint Agent  | C++17, WinHTTP, SetupAPI, WinAPI  |
| Backend API     | Python 3.11, FastAPI, aiosqlite   |
| Database        | SQLite (via aiosqlite)            |
| Authentication  | JWT (PyJWT), bcrypt               |
| Real-time       | WebSockets (FastAPI native)       |
| Frontend        | React 18, TailwindCSS, Chart.js   |
| HTTP Client     | WinHTTP (built-in Windows)        |

---

*SLIIT — Department of Computer System Engineering*
*B.Sc. (Hons) IT, Specialization in Cyber Security — 2026*
