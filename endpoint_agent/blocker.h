#pragma once
#ifndef BLOCKER_H
#define BLOCKER_H
#include <string>
bool blockUsbDevice(const std::string& vid, const std::string& pid, const std::string& name);
bool unblockUsbDevice(const std::string& vid, const std::string& pid, const std::string& name);
void enforceBlockedDevicesOnStartup();
#endif
