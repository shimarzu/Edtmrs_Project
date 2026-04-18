#pragma once
// EDTMRS v2.0 - Device Monitor Header
// Uses RegisterDeviceNotification + real hidden window (NOT HWND_MESSAGE)
// HWND_MESSAGE windows silently drop WM_DEVICECHANGE on many Windows 10/11 systems

#ifndef DEVICE_MONITOR_H
#define DEVICE_MONITOR_H

#include <windows.h>
#include <string>
#include <functional>

#define EDTMRS_VERSION "6.0.0"

struct DeviceInfo {
    std::string vendor_id;
    std::string product_id;
    std::string serial_number;
    std::string device_name;
    std::string device_type;
    std::string drive_letter;
    std::string hostname;
    std::string username;
    std::string timestamp;
    std::string agent_version;
};

using DeviceCallback = std::function<void(const DeviceInfo&)>;

class DeviceMonitor {
public:
    DeviceMonitor();
    ~DeviceMonitor();

    void start(DeviceCallback callback);
    void stop();

    static DeviceInfo   collectDeviceInfo(char driveLetter);
    static std::string  getHostname();
    static std::string  getUsername();
    static std::string  getTimestamp();

private:
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
    static DeviceCallback   s_callback;
    static HWND             s_hwnd;
    bool m_running;

    static std::string getDriveLetterFromMask(DWORD unitmask);
    static std::string queryDeviceRegistry(const std::string& path, const std::string& prop);
    static std::string getDeviceNameFromDrive(char driveLetter);
    static std::string getVendorIdFromDrive(char driveLetter);
    static std::string getProductIdFromDrive(char driveLetter);
    static std::string getSerialNumberFromDrive(char driveLetter);
};

#endif
