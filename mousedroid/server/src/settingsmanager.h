#pragma once

#include <string>
#include <iostream>
#include <fstream>
#include <map>

#include "input/inputmanager.hpp"
#include "logger.h"

#ifdef _WIN32
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include "windows.h"
#endif

class SettingsManager
{
    public:
        SettingsManager();

        void SetRunAtStartup(bool enabled);
        void SetMinimizeToTaskbar(bool enabled);
        bool GetRunAtStartup();
        bool GetMinToTaskbar();
        void SetMoveSensitivity(int value);
        void SetScrollSensitivity(int value);
        int GetMoveSensitivity();
        int GetScrollSensitivity();

    private:
        std::ifstream fin;
        std::map<std::string, std::string> settings;

        bool bRunAtStartup;
        bool bMinToTaskbar;

        std::wstring GetFilePath();
        void LoadConfigs();
        void SaveConfigs();
};