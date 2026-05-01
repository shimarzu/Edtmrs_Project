// EDTMRS v6.1 - Device Monitor
// Drive polling method - works on all Windows 10/11
// MinGW compatible - no problematic GUIDs

#include "device_monitor.h"
#include <setupapi.h>
#include <cfgmgr32.h>
#include <lmcons.h>
#include <iostream>
#include <algorithm>
#include <regex>
#include <set>
#include <thread>
#include <chrono>

// GUID_DEVCLASS_DISKDRIVE defined manually for MinGW compatibility
static const GUID MY_GUID_DEVCLASS_DISKDRIVE = {
    0x4D36E967, 0xE325, 0x11CE,
    { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 }
};

DeviceCallback DeviceMonitor::s_callback = nullptr;
DeviceMonitor::DeviceMonitor() : m_running(false) {}
DeviceMonitor::~DeviceMonitor() { stop(); }
void DeviceMonitor::stop() { m_running = false; }

std::string DeviceMonitor::getHostname() {
    char buf[256] = {}; DWORD sz = sizeof(buf);
    return GetComputerNameA(buf, &sz) ? std::string(buf) : "UNKNOWN_HOST";
}

std::string DeviceMonitor::getUsername() {
    char buf[UNLEN+1] = {}; DWORD sz = UNLEN+1;
    return GetUserNameA(buf, &sz) ? std::string(buf) : "UNKNOWN_USER";
}

std::string DeviceMonitor::getTimestamp() {
    SYSTEMTIME st; GetSystemTime(&st);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return std::string(buf);
}

std::string DeviceMonitor::queryDeviceRegistry(const std::string& path, const std::string& prop) {
    if (prop == "VID") {
        std::regex re("VID_([0-9A-Fa-f]{4})", std::regex::icase);
        std::smatch m;
        if (std::regex_search(path, m, re)) return m[1].str();
    }
    if (prop == "PID") {
        std::regex re("PID_([0-9A-Fa-f]{4})", std::regex::icase);
        std::smatch m;
        if (std::regex_search(path, m, re)) return m[1].str();
    }
    if (prop == "SERIAL") {
        size_t pos = path.rfind('\\');
        if (pos != std::string::npos) {
            std::string s = path.substr(pos+1);
            if (s.find('&') == std::string::npos && s.length() > 2) return s;
        }
    }
    return "unknown";
}

DeviceInfo DeviceMonitor::collectDeviceInfo(char driveLetter) {
    DeviceInfo info;
    info.drive_letter  = std::string(1, driveLetter) + ":";
    info.hostname      = getHostname();
    info.username      = getUsername();
    info.timestamp     = getTimestamp();
    info.agent_version = EDTMRS_VERSION;
    info.device_type   = "USB Storage";

    // Walk SetupAPI to get VID, PID, serial, name
    HDEVINFO h = SetupDiGetClassDevsA(&MY_GUID_DEVCLASS_DISKDRIVE,
                                       nullptr, nullptr, DIGCF_PRESENT);
    if (h == INVALID_HANDLE_VALUE) {
        info.device_name = "USB Storage Device";
        return info;
    }

    SP_DEVINFO_DATA dd; dd.cbSize = sizeof(dd);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(h, i, &dd); i++) {
        char id[512] = {};
        CM_Get_Device_IDA(dd.DevInst, id, sizeof(id), 0);
        std::string inst(id);
        std::string instUp = inst;
        std::transform(instUp.begin(), instUp.end(), instUp.begin(), ::toupper);

        if (instUp.find("USB") == std::string::npos &&
            instUp.find("USBSTOR") == std::string::npos) continue;

        // Device name
        char buf[512] = {};
        if (SetupDiGetDeviceRegistryPropertyA(h, &dd, SPDRP_FRIENDLYNAME,
            nullptr, (PBYTE)buf, sizeof(buf), nullptr) && buf[0])
            info.device_name = std::string(buf);
        else if (SetupDiGetDeviceRegistryPropertyA(h, &dd, SPDRP_DEVICEDESC,
            nullptr, (PBYTE)buf, sizeof(buf), nullptr) && buf[0])
            info.device_name = std::string(buf);

        // Serial number from USBSTOR instance
        if (instUp.find("USBSTOR") != std::string::npos) {
            std::string s = queryDeviceRegistry(instUp, "SERIAL");
            if (s != "unknown") {
                size_t amp = s.find('&');
                if (amp != std::string::npos) s = s.substr(0, amp);
                info.serial_number = s;
            }
        }

        // VID + PID from USB parent
        DEVINST parent; char pid[512] = {};
        if (CM_Get_Parent(&parent, dd.DevInst, 0) == CR_SUCCESS) {
            CM_Get_Device_IDA(parent, pid, sizeof(pid), 0);
            std::string ps(pid);
            std::transform(ps.begin(), ps.end(), ps.begin(), ::toupper);
            if (ps.find("USB\\VID") != std::string::npos) {
                std::string v = queryDeviceRegistry(ps, "VID");
                std::string p = queryDeviceRegistry(ps, "PID");
                if (v != "unknown") info.vendor_id  = v;
                if (p != "unknown") info.product_id = p;
            }
        }

        // Stop after first USB device found
        if (info.vendor_id != "unknown") break;
    }
    SetupDiDestroyDeviceInfoList(h);
    return info;
}

static std::set<char> getCurrentDrives() {
    std::set<char> drives;
    DWORD mask = GetLogicalDrives();
    for (int i = 2; i < 26; i++) {
        if (!(mask & (1u << i))) continue;
        char letter = 'A' + i;
        char root[4] = {letter, ':', '\\', '\0'};
        UINT type = GetDriveTypeA(root);
        if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED) continue;
        DWORD spc, bps, fc, tc;
        if (GetDiskFreeSpaceA(root, &spc, &bps, &fc, &tc))
            drives.insert(letter);
    }
    return drives;
}

void DeviceMonitor::start(DeviceCallback callback) {
    s_callback = callback;
    m_running  = true;
    std::cout << "[EDTMRS] Monitoring started (polling every 1s)" << std::endl;
    std::set<char> known = getCurrentDrives();
    std::cout << "[EDTMRS] Baseline drives: ";
    for (char c : known) std::cout << c << ":  ";
    std::cout << "\n[EDTMRS] INSERT USB TO TEST >>>" << std::endl;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        std::set<char> current = getCurrentDrives();

        // New drives
        for (char d : current) {
            if (!known.count(d)) {
                std::cout << "[EDTMRS] NEW DRIVE: " << d << ":" << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));
                if (s_callback) {
                    DeviceInfo info = collectDeviceInfo(d);
                    s_callback(info);
                }
                known.insert(d);
            }
        }
        // Removed drives
        std::vector<char> removed;
        for (char d : known)
            if (!current.count(d)) removed.push_back(d);
        for (char d : removed) {
            std::cout << "[EDTMRS] Drive removed: " << d << ":" << std::endl;
            known.erase(d);
        }
    }
    std::cout << "[EDTMRS] Monitor stopped." << std::endl;
}
