// EDTMRS v6.1 - Endpoint Agent
// - Polls drives every 1 second for USB insertions
// - Scans files for malware (autorun.inf, exe, bat, vbs etc)
// - Sends telemetry to admin server via HTTP POST
// - Polls server every 5s for block/unblock commands
// - Runs as silent Windows Service (auto-start on boot)
// - Writes log to edtmrs_agent.log

#include <windows.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

#include "device_monitor.h"
#include "http_client.h"
#include "blocker.h"

// ── Config ────────────────────────────────────────────────────────────────────
static std::string SERVER_HOST            = "127.0.0.1";
static int         SERVER_PORT            = 8000;
static int         HEARTBEAT_INTERVAL_SEC = 30;

// ── Exe directory ─────────────────────────────────────────────────────────────
static std::string getExeDir() {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string full(path);
    size_t pos = full.rfind('\\');
    return (pos != std::string::npos) ? full.substr(0, pos+1) : "";
}

// ── Logger ────────────────────────────────────────────────────────────────────
static void writeLog(const std::string& msg) {
    std::string logPath = getExeDir() + "edtmrs_agent.log";
    std::ofstream f(logPath, std::ios::app);
    if (f.is_open()) {
        SYSTEMTIME st; GetLocalTime(&st);
        char ts[32];
        snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
        f << "[" << ts << "] " << msg << "\n";
        f.flush();
    }
    std::cout << "[EDTMRS] " << msg << std::endl;
}

// ── Load config.ini ───────────────────────────────────────────────────────────
static void loadConfig() {
    std::string configPath = getExeDir() + "config.ini";
    std::ifstream f(configPath);
    if (!f.is_open()) f.open("config.ini");
    if (!f.is_open()) { writeLog("config.ini not found, using defaults"); return; }
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq+1);
        auto trim = [](std::string& s) {
            while (!s.empty() && (s.front()==' '||s.front()=='\t'||s.front()=='\r')) s.erase(s.begin());
            while (!s.empty() && (s.back()==' '||s.back()=='\t'||s.back()=='\r'||s.back()=='\n')) s.pop_back();
        };
        trim(key); trim(val);
        if      (key=="SERVER_HOST")        SERVER_HOST = val;
        else if (key=="SERVER_PORT")        SERVER_PORT = std::stoi(val);
        else if (key=="HEARTBEAT_INTERVAL") HEARTBEAT_INTERVAL_SEC = std::stoi(val);
    }
    writeLog("Config: " + SERVER_HOST + ":" + std::to_string(SERVER_PORT));
}

// ── File scanner ──────────────────────────────────────────────────────────────
static const std::vector<std::string> MALWARE_EXT = {
    ".exe",".bat",".cmd",".scr",".pif",".com",".vbs",".vbe",
    ".js",".jse",".wsf",".wsh",".ps1",".reg",".hta",".inf",".lnk"
};

static bool isMalicious(const std::string& name) {
    std::string low = name;
    std::transform(low.begin(), low.end(), low.begin(), ::tolower);
    if (low == "autorun.inf") return true;
    for (auto& ext : MALWARE_EXT)
        if (low.size() >= ext.size() &&
            low.substr(low.size()-ext.size()) == ext) return true;
    return false;
}

static void scanDrive(const std::string& driveLetter,
                      std::vector<std::string>& dangerous, int& total) {
    dangerous.clear(); total = 0;
    if (driveLetter.empty()) return;
    std::string searchPath = driveLetter + "\\*";
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        std::string nm(ffd.cFileName);
        if (nm == "." || nm == "..") continue;
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            total++;
            if (isMalicious(nm)) {
                dangerous.push_back(nm);
                writeLog("DANGEROUS FILE: " + nm);
            }
        }
    } while (FindNextFileA(hFind, &ffd));
    FindClose(hFind);
    writeLog("Scan done: " + std::to_string(total) + " files, " +
             std::to_string(dangerous.size()) + " dangerous");
}

// ── JSON builder ──────────────────────────────────────────────────────────────
static std::string esc(const std::string& s) {
    std::string o;
    for (char c : s) {
        if      (c=='"')  o+="\\\"";
        else if (c=='\\') o+="\\\\";
        else if (c=='\n') o+="\\n";
        else if (c=='\r') o+="\\r";
        else              o+=c;
    }
    return o;
}

static std::string buildJson(const DeviceInfo& d) {
    std::string arr = "[";
    for (size_t i = 0; i < d.dangerous_files.size(); i++) {
        if (i > 0) arr += ",";
        arr += "\"" + esc(d.dangerous_files[i]) + "\"";
    }
    arr += "]";
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
      << "\"agent_version\":\"" << esc(d.agent_version) << "\","
      << "\"dangerous_files\":" << arr                  << ","
      << "\"file_count\":"      << d.file_count
      << "}";
    return j.str();
}

// ── Parse string from simple JSON ────────────────────────────────────────────
static std::string parseJsonStr(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";
    size_t start = pos + search.size();
    size_t end   = json.find("\"", start);
    if (end == std::string::npos) return "";
    return json.substr(start, end-start);
}

// ── Device inserted callback ──────────────────────────────────────────────────
static void onDeviceInserted(const DeviceInfo& rawInfo) {
    writeLog("============================================");
    writeLog("USB DETECTED: " + rawInfo.device_name);
    writeLog("  VID=" + rawInfo.vendor_id + " PID=" + rawInfo.product_id);
    writeLog("  Drive=" + rawInfo.drive_letter + " Serial=" + rawInfo.serial_number);
    writeLog("  Host=" + rawInfo.hostname + " User=" + rawInfo.username);

    // Scan drive for malware
    DeviceInfo info = rawInfo;
    if (!rawInfo.drive_letter.empty()) {
        writeLog("Scanning " + rawInfo.drive_letter + " for malicious files...");
        scanDrive(rawInfo.drive_letter, info.dangerous_files, info.file_count);
        if (info.dangerous_files.empty())
            writeLog("Clean (" + std::to_string(info.file_count) + " files scanned)");
        else
            writeLog("MALICIOUS FILES FOUND: " + std::to_string(info.dangerous_files.size()));
    }

    // Send to server
    HttpClient client(SERVER_HOST, SERVER_PORT);
    HttpResponse resp = client.post("/api/device-event", buildJson(info));

    if (!resp.success) {
        writeLog("Server unreachable [HTTP " + std::to_string(resp.status_code) + "]");
        writeLog("============================================");
        return;
    }

    writeLog("Server: " + resp.body);
    std::string action    = parseJsonStr(resp.body, "action");
    std::string risk      = parseJsonStr(resp.body, "risk_level");
    writeLog("Risk=" + risk + " Action=" + action);

    // Parse log_id
    std::string log_id_str = "0";
    size_t p = resp.body.find("\"log_id\":");
    if (p != std::string::npos) {
        size_t s = p+9, e = resp.body.find_first_of(",}", s);
        log_id_str = resp.body.substr(s, e-s);
    }

    if (action == "block" || risk == "critical") {
        writeLog(">>> BLOCKING USB <<<");
        bool ok = blockUsbDevice(info.vendor_id, info.product_id, info.device_name);
        if (ok) {
            writeLog("USB BLOCKED - device is now inaccessible");
            std::string notify = "{\"log_id\":" + log_id_str +
                ",\"action_result\":\"blocked\",\"hostname\":\"" + info.hostname + "\"}";
            client.post("/api/action-result", notify);
        } else {
            writeLog("Block failed - run agent as Administrator!");
        }
    } else if (action == "allow") {
        writeLog(">>> UNBLOCKING USB <<<");
        if (unblockUsbDevice(info.vendor_id, info.product_id, info.device_name))
            writeLog("USB UNBLOCKED");
    }
    writeLog("============================================");
}

// ── Heartbeat + poll pending actions ─────────────────────────────────────────
static void pollPendingActions(const std::string& hostname) {
    HttpClient client(SERVER_HOST, SERVER_PORT);
    HttpResponse resp = client.get("/api/pending-actions/" + hostname);
    if (!resp.success) return;

    std::string body = resp.body;
    size_t start = body.find('[');
    size_t end   = body.rfind(']');
    if (start == std::string::npos || end == std::string::npos) return;

    size_t pos = start;
    while (pos < end) {
        size_t os = body.find('{', pos);
        if (os == std::string::npos || os >= end) break;
        size_t oe = body.find('}', os);
        if (oe == std::string::npos) break;
        std::string obj = body.substr(os, oe-os+1);

        std::string action = parseJsonStr(obj, "action");
        std::string vid    = parseJsonStr(obj, "vendor_id");
        std::string pid    = parseJsonStr(obj, "product_id");
        std::string name   = parseJsonStr(obj, "device_name");

        if (!action.empty() && !vid.empty()) {
            writeLog("PENDING ACTION: " + action + " VID=" + vid + " PID=" + pid);
            if (action == "block")
                blockUsbDevice(vid, pid, name.empty() ? "USB Device" : name);
            else if (action == "allow")
                unblockUsbDevice(vid, pid, name.empty() ? "USB Device" : name);
        }
        pos = oe+1;
    }
}

static void heartbeatLoop() {
    std::string hostname = DeviceMonitor::getHostname();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    while (true) {
        HttpClient client(SERVER_HOST, SERVER_PORT);
        client.heartbeat(hostname);
        pollPendingActions(hostname);
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

// ── Windows Service ───────────────────────────────────────────────────────────
static SERVICE_STATUS        g_status = {};
static SERVICE_STATUS_HANDLE g_handle = nullptr;
static DeviceMonitor*        g_mon    = nullptr;

VOID WINAPI SvcCtrlHandler(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
        writeLog("Service stopping...");
        g_status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_handle, &g_status);
        if (g_mon) g_mon->stop();
        g_status.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_handle, &g_status);
    }
}

VOID WINAPI SvcMain(DWORD, LPSTR*) {
    g_handle = RegisterServiceCtrlHandlerA("EDTMRSAgent", SvcCtrlHandler);
    if (!g_handle) return;
    g_status.dwServiceType      = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState     = SERVICE_START_PENDING;
    g_status.dwControlsAccepted = 0;
    SetServiceStatus(g_handle, &g_status);

    loadConfig();
    writeLog("=== EDTMRS Agent v" EDTMRS_VERSION " Service Starting ===");
    writeLog("Server: " + SERVER_HOST + ":" + std::to_string(SERVER_PORT));
    enforceBlockedDevicesOnStartup();

    g_status.dwCurrentState     = SERVICE_RUNNING;
    g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_handle, &g_status);

    std::thread hb(heartbeatLoop); hb.detach();
    writeLog("Monitoring USB devices silently...");

    DeviceMonitor mon;
    g_mon = &mon;
    mon.start(onDeviceInserted);

    writeLog("=== Service Stopped ===");
    g_status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_handle, &g_status);
}

// ── Main ──────────────────────────────────────────────────────────────────────
int main(int argc, char* argv[]) {
    loadConfig();

    if (argc > 1) {
        std::string arg(argv[1]);

        // Install service
        if (arg == "--install-service") {
            SC_HANDLE mgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
            if (!mgr) { std::cerr << "Run as Administrator!\n"; return 1; }
            char exe[MAX_PATH]; GetModuleFileNameA(nullptr, exe, MAX_PATH);
            std::string bin = std::string("\"") + exe + "\" --service";
            // Remove old
            SC_HANDLE old = OpenServiceA(mgr, "EDTMRSAgent", SERVICE_STOP|DELETE);
            if (old) { SERVICE_STATUS ss; ControlService(old, SERVICE_CONTROL_STOP, &ss);
                       Sleep(1000); DeleteService(old); CloseServiceHandle(old); Sleep(500); }
            SC_HANDLE svc = CreateServiceA(mgr, "EDTMRSAgent",
                "EDTMRS Endpoint Security Agent",
                SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
                SERVICE_AUTO_START, SERVICE_ERROR_NORMAL, bin.c_str(),
                nullptr, nullptr, nullptr, nullptr, nullptr);
            if (svc) {
                SERVICE_FAILURE_ACTIONSA fa = {};
                SC_ACTION acts[3] = {{SC_ACTION_RESTART,5000},{SC_ACTION_RESTART,10000},{SC_ACTION_RESTART,30000}};
                fa.dwResetPeriod=86400; fa.cActions=3; fa.lpsaActions=acts;
                ChangeServiceConfig2A(svc, SERVICE_CONFIG_FAILURE_ACTIONS, &fa);
                std::cout << "Service installed!\n";
                if (StartServiceA(svc, 0, nullptr)) std::cout << "Service started!\n";
                else std::cout << "Will start on next reboot.\n";
                CloseServiceHandle(svc);
            } else {
                std::cerr << "Install failed: " << GetLastError() << "\n";
            }
            CloseServiceHandle(mgr); return 0;
        }

        // Remove service
        if (arg == "--remove-service") {
            SC_HANDLE mgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
            SC_HANDLE svc = OpenServiceA(mgr, "EDTMRSAgent", SERVICE_STOP|DELETE);
            if (svc) { SERVICE_STATUS ss; ControlService(svc, SERVICE_CONTROL_STOP, &ss);
                       Sleep(2000); DeleteService(svc); CloseServiceHandle(svc);
                       std::cout << "Service removed.\n"; }
            else std::cout << "Service not found.\n";
            CloseServiceHandle(mgr); return 0;
        }

        // SCM entry point (called by Windows on boot)
        if (arg == "--service") {
            char svcName[] = "EDTMRSAgent";
            SERVICE_TABLE_ENTRYA tbl[] = {{svcName, SvcMain},{nullptr,nullptr}};
            StartServiceCtrlDispatcherA(tbl);
            return 0;
        }

        // Status
        if (arg == "--status") {
            SC_HANDLE mgr = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
            SC_HANDLE svc = OpenServiceA(mgr, "EDTMRSAgent", SERVICE_QUERY_STATUS);
            if (svc) {
                SERVICE_STATUS ss; QueryServiceStatus(svc, &ss);
                std::cout << "EDTMRSAgent: "
                          << (ss.dwCurrentState==SERVICE_RUNNING ? "RUNNING" : "STOPPED") << "\n";
                CloseServiceHandle(svc);
            } else std::cout << "EDTMRSAgent: NOT INSTALLED\n";
            CloseServiceHandle(mgr); return 0;
        }
    }

    // Console / foreground mode (for testing)
    std::cout << "\n  ================================================\n"
              << "   EDTMRS Agent v" EDTMRS_VERSION "\n"
              << "  ================================================\n\n"
              << "  Server : " << SERVER_HOST << ":" << SERVER_PORT << "\n"
              << "  Host   : " << DeviceMonitor::getHostname() << "\n"
              << "  User   : " << DeviceMonitor::getUsername() << "\n"
              << "  Log    : " << getExeDir() << "edtmrs_agent.log\n\n"
              << "  Install as service (run as Admin):\n"
              << "    edtmrs_agent.exe --install-service\n\n"
              << "  INSERT A USB TO TEST >>>\n\n";

    enforceBlockedDevicesOnStartup();
    std::thread hb(heartbeatLoop); hb.detach();
    DeviceMonitor mon;
    mon.start(onDeviceInserted);
    return 0;
}
