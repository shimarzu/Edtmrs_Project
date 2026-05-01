# EDTMRS - Cleanup and Final Setup Guide

## 🗑️ Files/Folders to DELETE

Run these commands from PowerShell as Administrator:

### 1. Delete Python virtual environment (will be recreated)
```powershell
Remove-Item -Path "admin_server\venv" -Recurse -Force
```

### 2. Delete Python cache files
```powershell
Remove-Item -Path "admin_server\__pycache__" -Recurse -Force -ErrorAction SilentlyContinue
Get-ChildItem -Path "admin_server" -Recurse -Directory -Filter "__pycache__" | Remove-Item -Recurse -Force
```

### 3. Delete Node.js modules (will be recreated)
```powershell
Remove-Item -Path "dashboard\node_modules" -Recurse -Force
Remove-Item -Path "dashboard\package-lock.json" -Force
```

### 4. Delete old database (will be recreated with correct schema)
```powershell
Remove-Item -Path "admin_server\edtmrs.db" -Force
```

### 5. Delete temporary test files from Desktop
```powershell
Remove-Item -Path "C:\Users\USER\Desktop\check_db.py" -Force -ErrorAction SilentlyContinue
Remove-Item -Path "C:\Users\USER\Desktop\fix_database.py" -Force -ErrorAction SilentlyContinue
Remove-Item -Path "C:\Users\USER\Desktop\test_device_event.py" -Force -ErrorAction SilentlyContinue
```

---

## ✅ FINAL SETUP STEPS

### Step 1: Recreate Python Virtual Environment
```powershell
cd C:\Users\USER\Desktop\Edtmrs_Project-main\Edtmrs_Project-main\admin_server
python -m venv venv
.\venv\Scripts\Activate.ps1
pip install --upgrade pip
pip install -r requirements.txt
```

### Step 2: Initialize Fresh Database
The database will be auto-created when you start the server with the correct schema.

### Step 3: Install Dashboard Dependencies
```powershell
cd C:\Users\USER\Desktop\Edtmrs_Project-main\Edtmrs_Project-main\dashboard
npm install
```

### Step 4: Configure Dashboard
Create `dashboard\.env` file with your Admin PC IP:
```
REACT_APP_API_URL=http://127.0.0.1:8000
```
(For local testing use `127.0.0.1`, for network deployment use your actual IP like `192.168.1.20`)

---

## 🚀 RUNNING THE SYSTEM

### Terminal 1: Start Admin Server
```powershell
cd C:\Users\USER\Desktop\Edtmrs_Project-main\Edtmrs_Project-main\admin_server
.\venv\Scripts\Activate.ps1
python main.py
```

### Terminal 2: Start Dashboard
```powershell
cd C:\Users\USER\Desktop\Edtmrs_Project-main\Edtmrs_Project-main\dashboard
npm start
```

### Terminal 3: Start Endpoint Agent (on User PC)
```powershell
cd C:\edtmrs\endpoint_agent
.\edtmrs_agent.exe
```

Or install as Windows Service:
```powershell
cd C:\edtmrs\endpoint_agent
.\edtmrs_agent.exe --install-service
net start EDTMRSAgent
```

---

## 🔧 CONFIGURATION

### Endpoint Agent Config (`C:\edtmrs\endpoint_agent\config.ini`)
```ini
SERVER_HOST=127.0.0.1
SERVER_PORT=8000
HEARTBEAT_INTERVAL=30
```

**For network deployment:** Change `SERVER_HOST` to your Admin PC's IP address (e.g., `192.168.1.20`)

---

## 🧪 TESTING

1. Open browser: `http://localhost:3000`
2. Login with: **admin / Admin@1234**
3. Insert a USB drive into the PC running the agent
4. Watch the dashboard for real-time alerts!

---

## 📋 DEFAULT CREDENTIALS

```
Username: admin
Password: Admin@1234
```

**⚠️ CHANGE THIS IMMEDIATELY IN PRODUCTION!**

---

## 🎯 EXPECTED BEHAVIOR

### When USB is inserted:
1. Agent detects device within 1-2 seconds
2. Agent sends HTTP POST to `/api/device-event`
3. Server processes event and classifies risk
4. Alert appears on dashboard via WebSocket (real-time)
5. Device shows in "Device Monitor" page
6. Stats update on main dashboard

### Agent Log Location:
```
C:\edtmrs\endpoint_agent\edtmrs_agent.log
```

Watch live:
```powershell
Get-Content C:\edtmrs\endpoint_agent\edtmrs_agent.log -Wait -Tail 10
```

---

## 🔥 TROUBLESHOOTING

### Agent shows "Event send FAILED [HTTP 500]"
- **Cause:** Database schema issue
- **Fix:** Delete `edtmrs.db` and restart server

### Dashboard can't connect to server
- **Check:** Server is running on port 8000
- **Check:** `.env` file has correct API URL
- **Check:** Firewall allows port 8000

### Agent can't connect to server
- **Check:** `config.ini` has correct SERVER_HOST
- **Check:** Both PCs on same network
- **Check:** Firewall allows inbound connections on port 8000

### USB not detected
- **Check:** Agent is running (console or service)
- **Check:** Agent running as Administrator (required for blocking)
- **Check:** Windows recognizes the USB drive in File Explorer first

---

## 📁 FINAL PROJECT STRUCTURE

After cleanup, your structure should be:

```
Edtmrs_Project-main/
├── admin_server/
│   ├── venv/                    [RECREATED]
│   ├── edtmrs.db                [RECREATED]
│   ├── main.py
│   ├── database.py
│   ├── auth.py
│   ├── models.py
│   ├── api_routes.py
│   ├── websocket_manager.py
│   └── requirements.txt
│
├── dashboard/
│   ├── node_modules/            [RECREATED]
│   ├── public/
│   ├── src/
│   ├── .env                     [CREATE THIS]
│   ├── package.json
│   └── tailwind.config.js
│
├── endpoint_agent/              [At C:\edtmrs\endpoint_agent]
│   ├── edtmrs_agent.exe
│   ├── config.ini
│   ├── main.cpp
│   ├── device_monitor.cpp
│   ├── http_client.cpp
│   └── [other source files]
│
├── database/
│   └── schema.sql
│
├── scripts/
│   ├── build_agent.ps1
│   ├── setup_admin.ps1
│   └── start_server.py
│
├── README.md
└── CLEANUP_GUIDE.md             [THIS FILE]
```

---

*Final cleanup and setup guide created: April 30, 2026*
*Project Version: 6.0.0 - Production Ready*
