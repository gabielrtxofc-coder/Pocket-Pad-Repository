#pragma once

#include <wx/wx.h>
#include <wx/listctrl.h>

#include <sstream>
#include <string>

#define LOG(...) Logger::log(__VA_ARGS__)

namespace Logger {

extern wxListView* monitor;

void displayOutput(const std::string& message);

template <typename... Args>
void log(const Args&... args) {
    std::ostringstream message;
    (message << ... << args);
    displayOutput(message.str());
}

}  // namespace Logger
