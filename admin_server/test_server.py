"""
EDTMRS Server Test Script
Run this AFTER python main.py is running
It will test every endpoint and tell you exactly what's working
"""
import urllib.request, urllib.error, json, sys

BASE = "http://localhost:8000"

def post(url, data, token=None):
    req = urllib.request.Request(url,
        data=json.dumps(data).encode(),
        headers={"Content-Type": "application/json"})
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return json.loads(r.read()), r.status
    except urllib.error.HTTPError as e:
        return json.loads(e.read()), e.code
    except Exception as ex:
        return {"error": str(ex)}, 0

def get(url, token=None):
    req = urllib.request.Request(url)
    if token:
        req.add_header("Authorization", f"Bearer {token}")
    try:
        with urllib.request.urlopen(req, timeout=5) as r:
            return json.loads(r.read()), r.status
    except urllib.error.HTTPError as e:
        return json.loads(e.read()), e.code
    except Exception as ex:
        return {"error": str(ex)}, 0

print("\n" + "="*50)
print("  EDTMRS Server Diagnostic Test")
print("="*50 + "\n")

# 1. Health
data, code = get(f"{BASE}/health")
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"1. Health check:     {status} (HTTP {code})")
if code != 200:
    print("   Server is not running! Start with: python main.py")
    sys.exit(1)

# 2. Login
data, code = post(f"{BASE}/api/auth/login", {"username":"admin","password":"Admin@1234"})
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"2. Login:            {status} (HTTP {code})")
if code != 200:
    print(f"   Error: {data}")
    sys.exit(1)
token = data["access_token"]
print(f"   Token: {token[:40]}...")

# 3. Device event (simulate USB insert)
data, code = post(f"{BASE}/api/device-event", {
    "vendor_id":"13FE","product_id":"4300",
    "serial_number":"TEST123","device_name":"TEST USB Device",
    "device_type":"USB Storage","drive_letter":"D:",
    "hostname":"DIAGNOSTIC-PC","username":"testuser",
    "timestamp":"2026-01-01T00:00:00Z",
    "agent_version":"6.1.0","dangerous_files":[],"file_count":0
})
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"3. Device event:     {status} (HTTP {code}) → risk={data.get('risk_level')} action={data.get('action')}")

# 4. Get devices
data, code = get(f"{BASE}/api/devices", token)
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"4. Get devices:      {status} (HTTP {code}) → {data.get('total',0)} records")

# 5. Get alerts
data, code = get(f"{BASE}/api/alerts", token)
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"5. Get alerts:       {status} (HTTP {code}) → {len(data.get('alerts',[]))} alerts")

# 6. Stats
data, code = get(f"{BASE}/api/stats", token)
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"6. Stats:            {status} (HTTP {code})")
if code == 200:
    print(f"   Endpoints={data.get('total_endpoints')} Devices={data.get('total_devices')} Alerts={data.get('total_alerts')}")

# 7. Block device
data, code = post(f"{BASE}/api/block-device",
    {"vendor_id":"13FE","product_id":"4300","serial_number":"TEST123",
     "device_name":"TEST USB Device","reason":"diagnostic test"}, token)
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"7. Block device:     {status} (HTTP {code})")

# 8. Pending actions
data, code = get(f"{BASE}/api/pending-actions/DIAGNOSTIC-PC")
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"8. Pending actions:  {status} (HTTP {code}) → {len(data.get('actions',[]))} pending")

# 9. Whitelist
data, code = post(f"{BASE}/api/whitelist-device",
    {"vendor_id":"13FE","product_id":"4300","serial_number":"TEST123",
     "device_name":"TEST USB Device","notes":"diagnostic test"}, token)
status = "✅ PASS" if code == 200 else "❌ FAIL"
print(f"9. Whitelist device: {status} (HTTP {code})")

print("\n" + "="*50)
print("  If all PASS: server is working correctly")
print("  If dashboard still shows nothing:")
print("  - Delete node_modules and reinstall npm")
print("  - Check .env file has correct URL")
print("  - Open browser DevTools Console for JS errors")
print("="*50 + "\n")
