#pragma once
#ifndef DEVICE_MONITOR_H
#define DEVICE_MONITOR_H

#include <windows.h>
#include <string>
#include <vector>
#include <functional>

#define EDTMRS_VERSION "6.1.0"

struct DeviceInfo {
    std::string vendor_id     = "unknown";
    std::string product_id    = "unknown";
    std::string serial_number = "unknown";
    std::string device_name   = "USB Storage Device";
    std::string device_type   = "USB Storage";
    std::string drive_letter  = "";
    std::string hostname      = "";
    std::string username      = "";
    std::string timestamp     = "";
    std::string agent_version = EDTMRS_VERSION;
    std::vector<std::string> dangerous_files;
    int file_count = 0;
};

using DeviceCallback = std::function<void(const DeviceInfo&)>;

class DeviceMonitor {
public:
    DeviceMonitor();
    ~DeviceMonitor();
    void start(DeviceCallback callback);
    void stop();
    static std::string getHostname();
    static std::string getUsername();
    static std::string getTimestamp();
    static DeviceInfo  collectDeviceInfo(char driveLetter);
private:
    bool m_running;
    static DeviceCallback s_callback;
    static std::string queryDeviceRegistry(const std::string& path, const std::string& prop);
};

#endif
