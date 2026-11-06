#pragma once

#ifdef _WIN32

#include "adb/AdbCoordinator.h"

#include <chrono>
#include <string>
#include <vector>

namespace PocketPad::Adb {

std::wstring GetBundledAdbPath();
CommandResult RunAdbProcess(const std::wstring& executable,
                            const std::vector<std::string>& arguments,
                            std::chrono::milliseconds timeout);

}  // namespace PocketPad::Adb

#endif  // _WIN32
