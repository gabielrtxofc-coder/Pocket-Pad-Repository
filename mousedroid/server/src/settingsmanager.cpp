#include "settingsmanager.h"

#include <algorithm>
#include <vector>

SettingsManager::SettingsManager()
{
    #ifdef _WIN32
        char buff[128] = { 0 };
        DWORD buffSize = sizeof(buff);

        LONG s2 = RegGetValueW(HKEY_CURRENT_USER, L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", L"Mousedroid", RRF_RT_REG_SZ, nullptr, &buff, &buffSize);
        
        settings["RUN_STARTUP"] = std::to_string((s2 == ERROR_SUCCESS));
    #endif

    LoadConfigs();
}

void SettingsManager::LoadConfigs()
{
    std::ifstream fin("config.ini");

    std::string line;
    while(std::getline(fin, line))
    {
        size_t pos = line.find('=');
        std::string setting = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        settings[setting] = value; 
    }

    fin.close();

    InputManager::MOVE_SENSITIVITY = std::atoi(settings["MOVE_SENSITIVITY"].c_str());
    InputManager::SCROLL_SENSITIVITY = std::atoi(settings["SCROLL_SENSITIVITY"].c_str());
}

void SettingsManager::SaveConfigs()
{
    std::ofstream fout("config.ini");
    for(auto &pair : settings)
    {
        fout << pair.first << "=" << pair.second << "\n";
    }
}

void SettingsManager::SetRunAtStartup(bool enabled)
{
    #ifdef _WIN32
        if (enabled)
        {
            std::wstring progPath = GetFilePath();
            if (!progPath.empty())
            {
                progPath = L"\"" + progPath + L"\"";
                RegSetKeyValueW(HKEY_CURRENT_USER,
                    L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run",
                    L"Mousedroid", REG_SZ, progPath.c_str(),
                    static_cast<DWORD>((progPath.size() + 1) * sizeof(wchar_t)));
            }
        }
        else
        {
            RegDeleteKeyValueW(HKEY_CURRENT_USER,
                L"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run", L"Mousedroid");
        }
    #endif 
}

void SettingsManager::SetMinimizeToTaskbar(bool enabled)
{
    settings["MINIMIZE_TASKBAR"] = std::to_string(enabled);
    SaveConfigs();
}

void SettingsManager::SetMoveSensitivity(int value)
{
    settings["MOVE_SENSITIVITY"] = std::to_string(value);
    InputManager::MOVE_SENSITIVITY = value;
    SaveConfigs();
}

void SettingsManager::SetScrollSensitivity(int value)
{
    settings["SCROLL_SENSITIVITY"] = std::to_string(value);
    InputManager::SCROLL_SENSITIVITY = value;
    SaveConfigs();
}

std::wstring SettingsManager::GetFilePath()
{
#ifdef _WIN32
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size() - 1) return std::wstring(buffer.data(), length);
        if (buffer.size() >= 32768) return {};
        buffer.resize(std::min<size_t>(buffer.size() * 2, 32768));
    }
#else
    return {};
#endif
}

bool SettingsManager::GetRunAtStartup()
{
    return settings["RUN_STARTUP"] == "1";
}

bool SettingsManager::GetMinToTaskbar()
{
    return settings["MINIMIZE_TASKBAR"] == "1";
}

int SettingsManager::GetMoveSensitivity()
{
    return std::atoi(settings["MOVE_SENSITIVITY"].c_str());
}

int SettingsManager::GetScrollSensitivity()
{
    return std::atoi(settings["SCROLL_SENSITIVITY"].c_str());
}
