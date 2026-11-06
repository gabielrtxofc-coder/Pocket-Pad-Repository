#pragma once

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
#include "wx/wx.h"
#endif

#include "gui/wxmain.h"
#include "net/server.h"
#include "settingsmanager.h"
#include "logger.h"

class wxApplication : public wxApp, public Server::ConnectionListener {
public:
    wxApplication();
    ~wxApplication() override;

    bool OnInit() override;
    int OnExit() override;

    void OnDeviceConnected(std::string device) override;
    void OnDeviceDisconnected(std::string device) override;
    void OnAdbStateChanged(const PocketPad::Adb::Snapshot& state) override;

private:
    wxMain* main_frame = nullptr;
    Server* server = nullptr;

    INPUT_MANAGER inputManager;
    SettingsManager settings;

    void OnWindowCloseEvent(wxCloseEvent& evt);
    void UpdateDeviceList();
};
