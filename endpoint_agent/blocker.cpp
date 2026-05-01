// EDTMRS v6.1 - USB Blocker
// Uses PowerShell Disable/Enable-PnpDevice
// Works on all Windows 10/11, no extra tools needed

#include "blocker.h"
#include "device_monitor.h"
#include <windows.h>
#include <fstream>
#include <string>
#include <algorithm>

static std::string getExeDir() {
    char path[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    std::string full(path);
    size_t pos = full.rfind('\\');
    return (pos != std::string::npos) ? full.substr(0, pos+1) : "";
}

static void blogLog(const std::string& msg) {
    std::string logPath = getExeDir() + "edtmrs_agent.log";
    std::ofstream f(logPath, std::ios::app);
    if (!f.is_open()) return;
    SYSTEMTIME st; GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%04d-%02d-%02d %02d:%02d:%02d",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    f << "[" << ts << "] [BLOCKER] " << msg << "\n";
}

static int runPS(const std::string& script, std::string& output) {
    char tmp[MAX_PATH]; GetTempPathA(MAX_PATH, tmp);
    std::string scriptFile = std::string(tmp) + "edtmrs_ps.ps1";
    std::string outFile    = std::string(tmp) + "edtmrs_ps_out.txt";
    { std::ofstream f(scriptFile); f << script; }
    std::string cmd = "powershell.exe -NonInteractive -ExecutionPolicy Bypass "
                      "-WindowStyle Hidden -File \"" + scriptFile +
                      "\" > \"" + outFile + "\" 2>&1";
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    std::string cmdCopy = cmd;
    BOOL ok = CreateProcessA(nullptr, &cmdCopy[0], nullptr, nullptr,
        FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (!ok) return -1;
    WaitForSingleObject(pi.hProcess, 20000);
    DWORD exitCode = 0; GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    std::ifstream out(outFile);
    if (out.is_open()) output = std::string(std::istreambuf_iterator<char>(out), {});
    DeleteFileA(scriptFile.c_str()); DeleteFileA(outFile.c_str());
    return (int)exitCode;
}

static void regWrite(const std::string& val, const std::string& data) {
    std::string cmd = "reg add \"HKLM\\SOFTWARE\\EDTMRS\\Blocked\" /v \"" +
                      val + "\" /t REG_SZ /d \"" + data + "\" /f";
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (pi.hProcess) { WaitForSingleObject(pi.hProcess,5000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
}

static void regDelete(const std::string& val) {
    std::string cmd = "reg delete \"HKLM\\SOFTWARE\\EDTMRS\\Blocked\" /v \"" + val + "\" /f";
    STARTUPINFOA si = {}; si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    CreateProcessA(nullptr, &cmd[0], nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
    if (pi.hProcess) { WaitForSingleObject(pi.hProcess,5000);
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread); }
}

bool blockUsbDevice(const std::string& vid, const std::string& pid, const std::string& name) {
    blogLog("Blocking: " + name + " VID=" + vid + " PID=" + pid);
    if (vid == "unknown" || vid.empty()) { blogLog("Cannot block: VID unknown"); return false; }

    std::string script =
        "$devices = Get-PnpDevice -PresentOnly | Where-Object {"
        "  $_.HardwareID -match 'VID_" + vid + "' -and"
        "  $_.HardwareID -match 'PID_" + pid + "'}\n"
        "if ($devices) {\n"
        "  foreach ($d in $devices) { Disable-PnpDevice -InstanceId $d.InstanceId -Confirm:$false }\n"
        "  Write-Host 'BLOCKED_OK'\n"
        "} else { Write-Host 'NOT_FOUND' }";

    std::string output; runPS(script, output);
    blogLog("PS output: " + output);

    if (output.find("BLOCKED_OK") != std::string::npos) {
        blogLog("USB BLOCKED successfully");
        regWrite("VID_" + vid + "_PID_" + pid, name);
        return true;
    }
    blogLog("Block failed");
    return false;
}

bool unblockUsbDevice(const std::string& vid, const std::string& pid, const std::string& name) {
    blogLog("Unblocking: " + name + " VID=" + vid + " PID=" + pid);
    if (vid == "unknown" || vid.empty()) { blogLog("Cannot unblock: VID unknown"); return false; }

    std::string script =
        "$devices = Get-PnpDevice | Where-Object {"
        "  $_.HardwareID -match 'VID_" + vid + "' -and"
        "  $_.HardwareID -match 'PID_" + pid + "'}\n"
        "if ($devices) {\n"
        "  foreach ($d in $devices) { Enable-PnpDevice -InstanceId $d.InstanceId -Confirm:$false }\n"
        "  Write-Host 'UNBLOCKED_OK'\n"
        "} else { Write-Host 'NOT_FOUND' }";

    std::string output; runPS(script, output);
    blogLog("PS output: " + output);

    if (output.find("UNBLOCKED_OK") != std::string::npos) {
        blogLog("USB UNBLOCKED successfully");
        regDelete("VID_" + vid + "_PID_" + pid);
        return true;
    }
    blogLog("Unblock failed");
    return false;
}

void enforceBlockedDevicesOnStartup() {
    blogLog("Enforcing blocked devices from registry...");
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\EDTMRS\\Blocked",
        0, KEY_READ, &hKey) != ERROR_SUCCESS) return;
    char valName[512]; DWORD nameLen = sizeof(valName);
    char valData[512]; DWORD dataLen = sizeof(valData);
    DWORD idx = 0;
    while (RegEnumValueA(hKey, idx, valName, &nameLen, nullptr, nullptr,
        (LPBYTE)valData, &dataLen) == ERROR_SUCCESS) {
        std::string key(valName), data(valData);
        std::string vid = "unknown", pid = "unknown";
        size_t vp = key.find("VID_"), pp = key.find("_PID_");
        if (vp != std::string::npos && pp != std::string::npos) {
            vid = key.substr(vp+4, pp-vp-4);
            pid = key.substr(pp+5);
        }
        blogLog("Re-applying block: " + data + " VID=" + vid + " PID=" + pid);
        blockUsbDevice(vid, pid, data);
        nameLen = sizeof(valName); dataLen = sizeof(valData); idx++;
    }
    RegCloseKey(hKey);
}
