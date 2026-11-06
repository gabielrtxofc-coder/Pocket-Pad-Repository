#pragma once

#define APP_WIDTH 500
#define APP_HEIGHT 450

#define APP_MARGIN 10

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/msgdlg.h>
#include <wx/hyperlink.h>
#include <wx/choice.h>

#include <functional>
#include <string>
#include <vector>

#include "adb/AdbCoordinator.h"
#include "gui/wxdevicelist.h"
#include "settingsmanager.h"
#include "logger.h"
#include "gui/taskbar.h"

#define CHK_STARTUP 1001
#define CHK_MINTASK 1002
#define SL_MOVE 1003
#define SL_SCROLL 1004

#define HOST_NAME_TXT 1010
#define HOST_CONNECTION_TXT 1011

class wxMain : public wxFrame {
public:
    wxDeviceList* wxdevlist = nullptr;
    wxPanel* tab_log = nullptr;
    SettingsManager& settings;

    wxMain(SettingsManager& settings);
    ~wxMain();

    void SetHostInfo(std::string hostname, std::string listener);
    void SetAdbState(const PocketPad::Adb::Snapshot& snapshot);
    void SetAdbSelectionHandler(std::function<void(std::string)> handler);

private:
    wxPanel* tab_status = nullptr;
    wxPanel* tab_settings = nullptr;
    wxNotebook* wxnotebook = nullptr;
    wxMTaskbar* tb = nullptr;
    wxStaticText* adbStatus = nullptr;
    wxChoice* adbDeviceChoice = nullptr;
    std::vector<std::string> adbChoiceSerials;
    std::string preferredAdbSerial;
    std::function<void(std::string)> adbSelectionHandler;
    bool updatingAdbChoices = false;

    wxDECLARE_EVENT_TABLE();

    void InitStatusTab();
    void InitLogTab();
    void InitSettingsTab();

    void CheckboxCmdHandler(wxCommandEvent& evt);
    void SliderCmdHandler(wxScrollEvent& evt);
    void AdbChoiceHandler(wxCommandEvent& evt);

    void HideWindow(wxIconizeEvent& evt);
};
