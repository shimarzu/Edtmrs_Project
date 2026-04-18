// EDTMRS v4.0 - Device Monitor
// FIXED for MinGW/w64devkit compilation
// Fix 1: GUID_DEVCLASS_DISKDRIVE defined manually (MinGW doesn't link it from setupapi)
// Fix 2: Removed all GUIDs that cause undefined reference with MinGW
// Fix 3: Uses pure polling - no WM_DEVICECHANGE, no RegisterDeviceNotification
// Fix 4: Device info via WMI-style registry query - works without special GUIDs

#include "device_monitor.h"
#include <windows.h>
#include <setupapi.h>
#include <cfgmgr32.h>
#include <lmcons.h>
#include <iostream>
#include <algorithm>
#include <regex>
#include <set>
#include <thread>
#include <chrono>
#include <string>

#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "cfgmgr32.lib")

// ─── GUID fix for MinGW ───────────────────────────────────────────────────────
// MinGW does not export GUID_DEVCLASS_DISKDRIVE from setupapi.lib
// We define it manually here — this is the official Microsoft GUID value
// {4D36E967-E325-11CE-BFC1-08002BE10318}
static const GUID MY_GUID_DEVCLASS_DISKDRIVE = {
    0x4D36E967, 0xE325, 0x11CE,
    { 0xBF, 0xC1, 0x08, 0x00, 0x2B, 0xE1, 0x03, 0x18 }
};

DeviceCallback DeviceMonitor::s_callback = nullptr;
HWND           DeviceMonitor::s_hwnd     = nullptr;

DeviceMonitor::DeviceMonitor()  : m_running(false) {}
DeviceMonitor::~DeviceMonitor() { stop(); }

void DeviceMonitor::stop() {
    m_running = false;
}

// ─── Basic helpers ────────────────────────────────────────────────────────────

std::string DeviceMonitor::getHostname() {
    char buf[256] = {};
    DWORD sz = sizeof(buf);
    return GetComputerNameA(buf, &sz) ? std::string(buf) : "UNKNOWN_HOST";
}

std::string DeviceMonitor::getUsername() {
    char buf[UNLEN + 1] = {};
    DWORD sz = UNLEN + 1;
    return GetUserNameA(buf, &sz) ? std::string(buf) : "UNKNOWN_USER";
}

std::string DeviceMonitor::getTimestamp() {
    SYSTEMTIME st;
    GetSystemTime(&st);
    char buf[64];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02dZ",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond);
    return std::string(buf);
}

std::string DeviceMonitor::getDriveLetterFromMask(DWORD unitmask) {
    for (int i = 0; i < 26; i++) {
        if (unitmask & (1u << i)) {
            char letter[3] = { (char)('A' + i), ':', '\0' };
            return std::string(letter);
        }
    }
    return "?:";
}

// ─── Parse VID / PID / Serial from instance path ─────────────────────────────
// Example path: USBSTOR\DISK&VEN_SanDisk&PROD_Ultra&REV_1.00\4C530001...
// Parent path:  USB\VID_0781&PID_5590\4C530001...

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
            std::string s = path.substr(pos + 1);
            // Real serial numbers don't contain '&'
            if (s.find('&') == std::string::npos && s.length() > 2)
                return s;
        }
    }
    return "unknown";
}

// ─── Get device name using SetupAPI with manually defined GUID ────────────────

std::string DeviceMonitor::getDeviceNameFromDrive(char /*dl*/) {
    HDEVINFO h = SetupDiGetClassDevsA(
        &MY_GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr, DIGCF_PRESENT);
    if (h == INVALID_HANDLE_VALUE) return "USB Storage Device";

    SP_DEVINFO_DATA dd; dd.cbSize = sizeof(dd);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(h, i, &dd); i++) {
        char id[512] = {}, buf[512] = {};
        CM_Get_Device_IDA(dd.DevInst, id, sizeof(id), 0);
        std::string inst(id);
        std::transform(inst.begin(), inst.end(), inst.begin(), ::toupper);
        if (inst.find("USB") == std::string::npos) continue;

        if (SetupDiGetDeviceRegistryPropertyA(h, &dd, SPDRP_FRIENDLYNAME,
            nullptr, (PBYTE)buf, sizeof(buf), nullptr) && buf[0]) {
            SetupDiDestroyDeviceInfoList(h);
            return std::string(buf);
        }
        if (SetupDiGetDeviceRegistryPropertyA(h, &dd, SPDRP_DEVICEDESC,
            nullptr, (PBYTE)buf, sizeof(buf), nullptr) && buf[0]) {
            SetupDiDestroyDeviceInfoList(h);
            return std::string(buf);
        }
    }
    SetupDiDestroyDeviceInfoList(h);
    return "USB Storage Device";
}

std::string DeviceMonitor::getVendorIdFromDrive(char /*dl*/) {
    HDEVINFO h = SetupDiGetClassDevsA(
        &MY_GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr, DIGCF_PRESENT);
    if (h == INVALID_HANDLE_VALUE) return "unknown";

    SP_DEVINFO_DATA dd; dd.cbSize = sizeof(dd);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(h, i, &dd); i++) {
        DEVINST parent; char pid[512] = {};
        if (CM_Get_Parent(&parent, dd.DevInst, 0) != CR_SUCCESS) continue;
        CM_Get_Device_IDA(parent, pid, sizeof(pid), 0);
        std::string ps(pid);
        std::transform(ps.begin(), ps.end(), ps.begin(), ::toupper);
        if (ps.find("USB\\VID") != std::string::npos) {
            std::string v = queryDeviceRegistry(ps, "VID");
            if (v != "unknown") { SetupDiDestroyDeviceInfoList(h); return v; }
        }
    }
    SetupDiDestroyDeviceInfoList(h);
    return "unknown";
}

std::string DeviceMonitor::getProductIdFromDrive(char /*dl*/) {
    HDEVINFO h = SetupDiGetClassDevsA(
        &MY_GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr, DIGCF_PRESENT);
    if (h == INVALID_HANDLE_VALUE) return "unknown";

    SP_DEVINFO_DATA dd; dd.cbSize = sizeof(dd);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(h, i, &dd); i++) {
        DEVINST parent; char pid[512] = {};
        if (CM_Get_Parent(&parent, dd.DevInst, 0) != CR_SUCCESS) continue;
        CM_Get_Device_IDA(parent, pid, sizeof(pid), 0);
        std::string ps(pid);
        std::transform(ps.begin(), ps.end(), ps.begin(), ::toupper);
        if (ps.find("USB\\VID") != std::string::npos) {
            std::string p = queryDeviceRegistry(ps, "PID");
            if (p != "unknown") { SetupDiDestroyDeviceInfoList(h); return p; }
        }
    }
    SetupDiDestroyDeviceInfoList(h);
    return "unknown";
}

std::string DeviceMonitor::getSerialNumberFromDrive(char /*dl*/) {
    HDEVINFO h = SetupDiGetClassDevsA(
        &MY_GUID_DEVCLASS_DISKDRIVE, nullptr, nullptr, DIGCF_PRESENT);
    if (h == INVALID_HANDLE_VALUE) return "unknown";

    SP_DEVINFO_DATA dd; dd.cbSize = sizeof(dd);
    for (DWORD i = 0; SetupDiEnumDeviceInfo(h, i, &dd); i++) {
        char id[512] = {};
        CM_Get_Device_IDA(dd.DevInst, id, sizeof(id), 0);
        std::string inst(id);
        std::transform(inst.begin(), inst.end(), inst.begin(), ::toupper);
        if (inst.find("USBSTOR") == std::string::npos) continue;
        std::string s = queryDeviceRegistry(inst, "SERIAL");
        if (s != "unknown" && s.length() > 4) {
            size_t amp = s.find('&');
            if (amp != std::string::npos) s = s.substr(0, amp);
            SetupDiDestroyDeviceInfoList(h);
            return s;
        }
    }
    SetupDiDestroyDeviceInfoList(h);
    return "unknown";
}

// ─── Collect all device info for a given drive letter ────────────────────────

DeviceInfo DeviceMonitor::collectDeviceInfo(char driveLetter) {
    DeviceInfo info;
    info.drive_letter  = (driveLetter != '?')
        ? (std::string(1, driveLetter) + ":") : "N/A";
    info.hostname      = getHostname();
    info.username      = getUsername();
    info.timestamp     = getTimestamp();
    info.agent_version = EDTMRS_VERSION;
    info.device_type   = "USB Storage";
    info.vendor_id     = getVendorIdFromDrive(driveLetter);
    info.product_id    = getProductIdFromDrive(driveLetter);
    info.serial_number = getSerialNumberFromDrive(driveLetter);
    info.device_name   = getDeviceNameFromDrive(driveLetter);
    return info;
}

// ─── Get set of all currently accessible removable drives ────────────────────

static std::set<char> getCurrentDrives() {
    std::set<char> drives;
    DWORD mask = GetLogicalDrives();
    for (int i = 2; i < 26; i++) {          // Start from C: (skip A: B: floppy)
        if (!(mask & (1u << i))) continue;
        char letter = 'A' + i;
        char root[4] = { letter, ':', '\\', '\0' };
        UINT type = GetDriveTypeA(root);
        // Include removable (USB flash) and fixed (external USB HDD)
        if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED) continue;
        // Verify the drive is readable (not just mounted but empty)
        DWORD spc, bps, fc, tc;
        if (GetDiskFreeSpaceA(root, &spc, &bps, &fc, &tc)) {
            drives.insert(letter);
        }
    }
    return drives;
}

// ─── WndProc stub (not used in polling mode, kept for ABI compatibility) ─────
LRESULT CALLBACK DeviceMonitor::WndProc(HWND hwnd, UINT msg,
    WPARAM wParam, LPARAM lParam) {
    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

// ─── Main monitoring loop — DRIVE POLLING ────────────────────────────────────

void DeviceMonitor::start(DeviceCallback callback) {
    s_callback = callback;
    m_running  = true;

    std::cout << "[EDTMRS] Drive polling monitor started (1 second interval)" << std::endl;

    // Snapshot drives present at startup
    std::set<char> knownDrives = getCurrentDrives();

    std::cout << "[EDTMRS] Baseline drives present: ";
    if (knownDrives.empty()) {
        std::cout << "(none)";
    } else {
        for (char c : knownDrives) std::cout << c << ":  ";
    }
    std::cout << std::endl;
    std::cout << "[EDTMRS] Ready. >>> PLUG IN A USB DEVICE NOW <<<" << std::endl;

    // Poll every second
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

        std::set<char> currentDrives = getCurrentDrives();

        // ── Detect NEW drives ─────────────────────────────────────────────────
        for (char drive : currentDrives) {
            if (knownDrives.count(drive) == 0) {
                std::cout << "[EDTMRS] *** NEW DRIVE DETECTED: "
                          << drive << ": ***" << std::endl;

                // Give Windows 2 seconds to finish mounting
                std::this_thread::sleep_for(std::chrono::milliseconds(2000));

                if (s_callback) {
                    DeviceInfo info = collectDeviceInfo(drive);
                    s_callback(info);
                }
                knownDrives.insert(drive);
            }
        }

        // ── Detect REMOVED drives ─────────────────────────────────────────────
        std::vector<char> removed;
        for (char drive : knownDrives) {
            if (currentDrives.count(drive) == 0) {
                std::cout << "[EDTMRS] Drive removed: " << drive << ":" << std::endl;
                removed.push_back(drive);
            }
        }
        for (char drive : removed) knownDrives.erase(drive);
    }

    std::cout << "[EDTMRS] Monitor stopped." << std::endl;
}
