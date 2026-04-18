// EDTMRS v6.0 - Endpoint Agent Main
// REAL USB BLOCKING: Uses devcon/pnputil + registry to physically
// block/unblock specific USB devices by hardware ID
// Requires: Agent running as Administrator (Windows Service mode does this)

#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>
#include <algorithm>

#include "device_monitor.h"
#include "http_client.h"

// ─── Config ───────────────────────────────────────────────────────────────────
static std::string SERVER_HOST            = "127.0.0.1";
static int         SERVER_PORT            = 8000;
static int         HEARTBEAT_INTERVAL_SEC = 30;

// ─── Get exe directory ────────────────────────────────────────────────────────
static std::string getExeDir() {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string full(path);
    size_t pos = full.rfind('\\');
    return (pos != std::string::npos) ? full.substr(0, pos + 1) : "";
}

// ─── File logger ─────────────────────────────────────────────────────────────
static void writeLog(const std::string& msg) {
    std::string logPath = getExeDir() + "edtmrs_agent.log";
    std::ofstream log(logPath, std::ios::app);
    if (log.is_open()) {
        SYSTEMTIME st; GetLocalTime(&st);
        char ts[32];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        log << "[" << ts << "] " << msg << "\n";
        log.flush();
    }
    std::cout << "[EDTMRS] " << msg << std::endl;
}

// ─── Load config ──────────────────────────────────────────────────────────────
static void loadConfig() {
    std::string configPath = getExeDir() + "config.ini";
    std::ifstream f(configPath);
    if (!f.is_open()) f.open("config.ini");
    if (!f.is_open()) return;
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq), val = line.substr(eq + 1);
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.front()==' '||s.front()=='\t')) s.erase(s.begin());
            while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='\r'||s.back()=='\n')) s.pop_back();
        };
        trim(key); trim(val);
        if      (key == "SERVER_HOST")        SERVER_HOST            = val;
        else if (key == "SERVER_PORT")        SERVER_PORT            = std::stoi(val);
        else if (key == "HEARTBEAT_INTERVAL") HEARTBEAT_INTERVAL_SEC = std::stoi(val);
    }
    writeLog("Config: SERVER=" + SERVER_HOST + ":" + std::to_string(SERVER_PORT));
}

// ─── JSON builder ─────────────────────────────────────────────────────────────
static std::string buildJson(const DeviceInfo& d) {
    auto esc = [](const std::string& s) {
        std::string o;
        for (char c : s) {
            if      (c=='"')  o += "\\\"";
            else if (c=='\\') o += "\\\\";
            else if (c=='\n') o += "\\n";
            else if (c=='\r') o += "\\r";
            else              o += c;
        }
        return o;
    };
    std::ostringstream j;
    j << "{"
      << "\"vendor_id\":\""     << esc(d.vendor_id)     << "\","
      << "\"product_id\":\""    << esc(d.product_id)    << "\","
      << "\"serial_number\":\"" << esc(d.serial_number) << "\","
      << "\"device_name\":\""   << esc(d.device_name)   << "\","
      << "\"device_type\":\""   << esc(d.device_type)   << "\","
      << "\"drive_letter\":\""  << esc(d.drive_letter)  << "\","
      << "\"hostname\":\""      << esc(d.hostname)      << "\","
      << "\"username\":\""      << esc(d.username)      << "\","
      << "\"timestamp\":\""     << esc(d.timestamp)     << "\","
      << "\"agent_version\":\"" << esc(d.agent_version) << "\""
      << "}";
    return j.str();
}

// ─── Parse JSON value ─────────────────────────────────────────────────────────
// Extracts value of a key from a simple flat JSON string
static std::string parseJsonValue(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        // Try without quotes for booleans/numbers
        search = "\"" + key + "\":";
        pos = json.find(search);
        if (pos == std::string::npos) return "";
        size_t start = pos + search.length();
        size_t end   = json.find_first_of(",}", start);
        return json.substr(start, end - start);
    }
    size_t start = pos + search.length();
    size_t end   = json.find("\"", start);
    return json.substr(start, end - start);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  REAL USB BLOCKING FUNCTIONS
//  Method: Use Windows built-in pnputil.exe to disable/enable devices by HwId
//  This physically disables the device — Windows shows "Device is disabled"
//  NO third-party tools needed. pnputil.exe ships with every Windows 7+
// ═══════════════════════════════════════════════════════════════════════════════

// Build Hardware ID string from VID+PID (format Windows uses)
static std::string buildHwId(const std::string& vid, const std::string& pid) {
    // Windows Hardware ID format: USB\VID_XXXX&PID_XXXX
    std::string v = vid, p = pid;
    std::transform(v.begin(), v.end(), v.begin(), ::toupper);
    std::transform(p.begin(), p.end(), p.begin(), ::toupper);
    return "USB\\VID_" + v + "&PID_" + p;
}

// Run a Windows command silently and return exit code
static int runCommand(const std::string& cmd) {
    writeLog("Running: " + cmd);
    // Create a hidden process
    STARTUPINFOA si = {};
    si.cb          = sizeof(si);
    si.dwFlags     = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    std::string cmdCopy = cmd; // CreateProcessA needs non-const

    BOOL ok = CreateProcessA(
        nullptr,
        &cmdCopy[0],
        nullptr, nullptr,
        FALSE,
        CREATE_NO_WINDOW,
        nullptr, nullptr,
        &si, &pi
    );

    if (!ok) {
        writeLog("CreateProcess failed: " + std::to_string(GetLastError()));
        return -1;
    }

    // Wait up to 15 seconds for command to complete
    WaitForSingleObject(pi.hProcess, 15000);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    writeLog("Command exit code: " + std::to_string(exitCode));
    return (int)exitCode;
}

// ── BLOCK a USB device physically ────────────────────────────────────────────
// Uses pnputil to disable the device node matching VID+PID
// After this, the USB drive becomes inaccessible (no drive letter assigned)
static bool blockUsbDevice(const DeviceInfo& info) {
    writeLog("=== BLOCKING DEVICE: " + info.device_name + " ===");
    writeLog("VID: " + info.vendor_id + " PID: " + info.product_id);

    if (info.vendor_id == "unknown" || info.vendor_id.empty()) {
        writeLog("Cannot block: VID is unknown");
        return false;
    }

    // Method 1: Use pnputil to disable all devices matching the hardware ID
    // pnputil /disable-device "USB\VID_XXXX&PID_XXXX" /subtree
    std::string hwId = buildHwId(info.vendor_id, info.product_id);
    std::string cmd1 = "pnputil /disable-device \"" + hwId + "\" /subtree";
    int result = runCommand(cmd1);

    if (result == 0) {
        writeLog("✅ Device BLOCKED via pnputil: " + hwId);

        // Also write to registry for persistence across reboots
        // Block this specific device class from being enabled
        std::string regCmd =
            "reg add \"HKLM\\SOFTWARE\\EDTMRS\\BlockedDevices\" "
            "/v \"" + hwId + "\" /t REG_SZ /d \"blocked\" /f";
        runCommand(regCmd);

        return true;
    }

    // Method 2 fallback: Use devcon if available
    std::string devconPath = getExeDir() + "devcon.exe";
    std::ifstream devconCheck(devconPath);
    if (devconCheck.good()) {
        devconCheck.close();
        std::string cmd2 = "\"" + devconPath + "\" disable \"" + hwId + "\"";
        result = runCommand(cmd2);
        if (result == 0) {
            writeLog("✅ Device BLOCKED via devcon: " + hwId);
            return true;
        }
    }

    // Method 3 fallback: Disable via registry (USBSTOR device specific)
    // This changes the device start type to 4 (disabled) for this specific device
    // Find the device instance path in registry and disable it
    writeLog("Attempting registry-based block...");

    // Use PowerShell to disable the device
    std::string psCmd =
        "powershell -NonInteractive -WindowStyle Hidden -Command \""
        "Get-PnpDevice | Where-Object { $_.HardwareID -like '*VID_" +
        info.vendor_id + "*PID_" + info.product_id + "*' } | "
        "Disable-PnpDevice -Confirm:$false\"";
    result = runCommand(psCmd);

    if (result == 0) {
        writeLog("✅ Device BLOCKED via PowerShell PnP");
        return true;
    }

    writeLog("❌ All block methods failed for " + hwId);
    return false;
}

// ── UNBLOCK (whitelist) a USB device ─────────────────────────────────────────
static bool unblockUsbDevice(const DeviceInfo& info) {
    writeLog("=== UNBLOCKING DEVICE: " + info.device_name + " ===");

    if (info.vendor_id == "unknown" || info.vendor_id.empty()) {
        writeLog("Cannot unblock: VID is unknown");
        return false;
    }

    std::string hwId = buildHwId(info.vendor_id, info.product_id);

    // Method 1: pnputil enable
    std::string cmd1 = "pnputil /enable-device \"" + hwId + "\" /subtree";
    int result = runCommand(cmd1);

    if (result == 0) {
        writeLog("✅ Device UNBLOCKED via pnputil: " + hwId);

        // Remove from registry block list
        std::string regCmd =
            "reg delete \"HKLM\\SOFTWARE\\EDTMRS\\BlockedDevices\" "
            "/v \"" + hwId + "\" /f";
        runCommand(regCmd);

        return true;
    }

    // Method 2: devcon enable
    std::string devconPath = getExeDir() + "devcon.exe";
    std::ifstream devconCheck(devconPath);
    if (devconCheck.good()) {
        devconCheck.close();
        std::string cmd2 = "\"" + devconPath + "\" enable \"" + hwId + "\"";
        result = runCommand(cmd2);
        if (result == 0) {
            writeLog("✅ Device UNBLOCKED via devcon: " + hwId);
            return true;
        }
    }

    // Method 3: PowerShell enable
    std::string psCmd =
        "powershell -NonInteractive -WindowStyle Hidden -Command \""
        "Get-PnpDevice | Where-Object { $_.HardwareID -like '*VID_" +
        info.vendor_id + "*PID_" + info.product_id + "*' } | "
        "Enable-PnpDevice -Confirm:$false\"";
    result = runCommand(psCmd);

    if (result == 0) {
        writeLog("✅ Device UNBLOCKED via PowerShell PnP");
        return true;
    }

    writeLog("❌ All unblock methods failed for " + hwId);
    return false;
}

// ── Check startup registry and block any previously blocked devices ───────────
static void enforceBlockedDevicesOnStartup() {
    writeLog("Checking for previously blocked devices to enforce...");

    // Read from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\EDTMRS\\BlockedDevices", 0, KEY_READ, &hKey) != ERROR_SUCCESS) {
        writeLog("No blocked devices registry key found.");
        return;
    }

    char valueName[512];
    DWORD nameLen = sizeof(valueName);
    DWORD index   = 0;

    while (RegEnumValueA(hKey, index, valueName, &nameLen,
        nullptr, nullptr, nullptr, nullptr) == ERROR_SUCCESS) {

        std::string hwId(valueName);
        writeLog("Re-applying block for: " + hwId);

        std::string cmd = "pnputil /disable-device \"" + hwId + "\" /subtree";
        runCommand(cmd);

        nameLen = sizeof(valueName);
        index++;
    }

    RegCloseKey(hKey);
    writeLog("Startup enforcement done.");
}

// ═══════════════════════════════════════════════════════════════════════════════
//  DEVICE EVENT HANDLER — reads server response and acts on it
// ═══════════════════════════════════════════════════════════════════════════════

static void onDeviceInserted(const DeviceInfo& info) {
    writeLog("============================================");
    writeLog("USB DEVICE DETECTED: " + info.device_name);
    writeLog("  Drive  : " + info.drive_letter);
    writeLog("  VID    : " + info.vendor_id);
    writeLog("  PID    : " + info.product_id);
    writeLog("  Serial : " + info.serial_number);
    writeLog("  Host   : " + info.hostname);
    writeLog("  User   : " + info.username);

    HttpClient client(SERVER_HOST, SERVER_PORT);
    HttpResponse resp = client.post("/api/device-event", buildJson(info));

    if (!resp.success) {
        writeLog("FAILED to send event [HTTP " + std::to_string(resp.status_code) + "]");
        writeLog("============================================");
        return;
    }

    writeLog("Server responded [HTTP " + std::to_string(resp.status_code) + "]: " + resp.body);

    // ── Parse server response ────────────────────────────────────────────────
    // Server returns: {"status":"ok","log_id":1,"risk_level":"critical","action":"block"}
    std::string action     = parseJsonValue(resp.body, "action");
    std::string risk_level = parseJsonValue(resp.body, "risk_level");

    writeLog("Risk level : " + risk_level);
    writeLog("Action     : " + (action.empty() ? "none" : action));

    // ── Take action based on server instruction ──────────────────────────────
    if (action == "block") {
        writeLog(">>> SERVER SAYS: BLOCK THIS DEVICE <<<");
        bool blocked = blockUsbDevice(info);
        if (blocked) {
            writeLog("✅ PHYSICAL BLOCK SUCCESSFUL — USB is now inaccessible");
            // Notify server that block was executed
            std::string notification =
                "{\"log_id\":" + parseJsonValue(resp.body, "log_id") +
                ",\"action_result\":\"blocked\",\"hostname\":\"" + info.hostname + "\"}";
            client.post("/api/action-result", notification);
        } else {
            writeLog("❌ Physical block failed — check agent has Administrator rights");
        }

    } else if (action == "allow" || action == "whitelist") {
        writeLog(">>> SERVER SAYS: ALLOW THIS DEVICE <<<");
        bool unblocked = unblockUsbDevice(info);
        if (unblocked) {
            writeLog("✅ DEVICE UNBLOCKED — USB is now accessible");
        }

    } else if (risk_level == "critical") {
        // Device is in blocked list but no explicit action sent
        // Auto-block it
        writeLog(">>> AUTO-BLOCK: Device is CRITICAL risk <<<");
        blockUsbDevice(info);

    } else {
        writeLog("No block action required for this device.");
    }

    writeLog("============================================");
}

// ─── Heartbeat ────────────────────────────────────────────────────────────────
static void heartbeatLoop() {
    std::string hostname = DeviceMonitor::getHostname();
    HttpClient  client(SERVER_HOST, SERVER_PORT);
    std::this_thread::sleep_for(std::chrono::seconds(5));
    while (true) {
        HttpResponse r = client.heartbeat(hostname);
        if (!r.success)
            writeLog("Heartbeat failed - server may be down");
        std::this_thread::sleep_for(std::chrono::seconds(HEARTBEAT_INTERVAL_SEC));
    }
}

// ─── Windows Service ──────────────────────────────────────────────────────────
static SERVICE_STATUS        g_svcStatus = {};
static SERVICE_STATUS_HANDLE g_svcHandle = nullptr;
static DeviceMonitor*        g_monitor   = nullptr;

VOID WINAPI SvcCtrlHandler(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
        writeLog("Service stop requested");
        g_svcStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_svcHandle, &g_svcStatus);
        if (g_monitor) g_monitor->stop();
        g_svcStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_svcHandle, &g_svcStatus);
    }
}

VOID WINAPI SvcMain(DWORD, LPSTR*) {
    g_svcHandle = RegisterServiceCtrlHandlerA("EDTMRSAgent", SvcCtrlHandler);
    if (!g_svcHandle) return;

    g_svcStatus.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    g_svcStatus.dwCurrentState     = SERVICE_START_PENDING;
    g_svcStatus.dwControlsAccepted = 0;
    SetServiceStatus(g_svcHandle, &g_svcStatus);

    loadConfig();
    writeLog("=== EDTMRS Agent v" + std::string(EDTMRS_VERSION) + " Service Starting ===");

    // Enforce any previously blocked devices on startup
    enforceBlockedDevicesOnStartup();

    g_svcStatus.dwCurrentState     = SERVICE_RUNNING;
    g_svcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_svcHandle, &g_svcStatus);

    std::thread hb(heartbeatLoop);
    hb.detach();

    writeLog("Monitoring USB devices...");
    DeviceMonitor monitor;
    g_monitor = &monitor;
    monitor.start(onDeviceInserted);

    writeLog("=== Service Stopped ===");
    g_svcStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_svcHandle, &g_svcStatus);
}

// ─── Main ─────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    loadConfig();

    if (argc > 1) {
        std::string arg(argv[1]);

        if (arg == "--install-service") {
            SC_HANDLE mgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
            if (!mgr) { std::cerr << "Run as Administrator!\n"; return 1; }
            char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, MAX_PATH);
            std::string binPath = std::string("\"") + exe + "\" --service";
            SC_HANDLE svc = CreateServiceA(mgr, "EDTMRSAgent", "EDTMRS Endpoint Security Agent",
                SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_AUTO_START,
                SERVICE_ERROR_NORMAL, binPath.c_str(),
                nullptr, nullptr, nullptr, nullptr, nullptr);
            if (svc) {
                SERVICE_DESCRIPTIONA desc;
                char descStr[] = "EDTMRS USB Device Threat Monitoring Agent - monitors and blocks unauthorized USB devices.";
                desc.lpDescription = descStr;
                ChangeServiceConfig2A(svc, SERVICE_CONFIG_DESCRIPTION, &desc);
                SERVICE_FAILURE_ACTIONSA fa = {};
                SC_ACTION acts[3] = {{SC_ACTION_RESTART,5000},{SC_ACTION_RESTART,10000},{SC_ACTION_RESTART,30000}};
                fa.dwResetPeriod=86400; fa.cActions=3; fa.lpsaActions=acts;
                ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fa);
                std::cout << "Service installed! Starting...\n";
                StartServiceA(svc, 0, nullptr);
                std::cout << "Agent running silently in background.\n";
                CloseServiceHandle(svc);
            } else {
                std::cerr << "Install failed: " << GetLastError() << "\n";
            }
            CloseServiceHandle(mgr);
            return 0;
        }

        if (arg == "--remove-service") {
            SC_HANDLE mgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
            SC_HANDLE svc = OpenServiceA(mgr, "EDTMRSAgent", SERVICE_STOP|DELETE);
            if (svc) {
                SERVICE_STATUS ss; ControlService(svc, SERVICE_CONTROL_STOP, &ss);
                Sleep(2000); DeleteService(svc);
                std::cout << "Service removed.\n"; CloseServiceHandle(svc);
            }
            CloseServiceHandle(mgr); return 0;
        }

        if (arg == "--service") {
            char svcName[] = "EDTMRSAgent";
            SERVICE_TABLE_ENTRYA tbl[] = {{svcName, SvcMain},{nullptr,nullptr}};
            StartServiceCtrlDispatcherA(tbl);
            return 0;
        }

        if (arg == "--status") {
            SC_HANDLE mgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
            SC_HANDLE svc = OpenServiceA(mgr, "EDTMRSAgent", SERVICE_QUERY_STATUS);
            if (svc) {
                SERVICE_STATUS ss; QueryServiceStatus(svc, &ss);
                std::string state;
                switch(ss.dwCurrentState) {
                    case SERVICE_RUNNING: state="RUNNING ✅"; break;
                    case SERVICE_STOPPED: state="STOPPED ❌"; break;
                    default: state="OTHER";
                }
                std::cout << "EDTMRSAgent: " << state << "\n";
                CloseServiceHandle(svc);
            } else { std::cout << "EDTMRSAgent: NOT INSTALLED\n"; }
            CloseServiceHandle(mgr); return 0;
        }
    }

    // Console/foreground mode
    std::cout << "\n  ===================================================\n";
    std::cout << "   EDTMRS Endpoint Agent v" << EDTMRS_VERSION << "\n";
    std::cout << "   REAL USB BLOCKING ENABLED\n";
    std::cout << "  ===================================================\n\n";
    std::cout << "  Server  : " << SERVER_HOST << ":" << SERVER_PORT << "\n";
    std::cout << "  Host    : " << DeviceMonitor::getHostname() << "\n";
    std::cout << "  User    : " << DeviceMonitor::getUsername() << "\n";
    std::cout << "  Log     : " << getExeDir() << "edtmrs_agent.log\n\n";
    std::cout << "  ⚠ Run as Administrator for blocking to work!\n\n";
    std::cout << "  >>> PLUG IN A USB DEVICE TO TEST <<<\n\n";

    // Enforce previously blocked devices
    enforceBlockedDevicesOnStartup();

    std::thread hb(heartbeatLoop);
    hb.detach();

    DeviceMonitor monitor;
    monitor.start(onDeviceInserted);
    return 0;
}
