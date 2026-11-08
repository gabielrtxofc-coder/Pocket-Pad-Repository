#include "wxmain.h"

#include <algorithm>
#include <iterator>
#include <utility>

#include "../res/icon.xpm"

wxBEGIN_EVENT_TABLE(wxMain, wxFrame)
    EVT_ICONIZE(wxMain::HideWindow)
    EVT_CHECKBOX(CHK_STARTUP, wxMain::CheckboxCmdHandler)
    EVT_CHECKBOX(CHK_MINTASK, wxMain::CheckboxCmdHandler)
    EVT_COMMAND_SCROLL(SL_MOVE, wxMain::SliderCmdHandler)
    EVT_COMMAND_SCROLL(SL_SCROLL, wxMain::SliderCmdHandler)
wxEND_EVENT_TABLE()

wxMain::wxMain(SettingsManager &_settings) : 
    wxFrame(NULL, wxID_ANY, "Mousedroid", wxDefaultPosition, wxSize(APP_WIDTH, APP_HEIGHT), wxDEFAULT_FRAME_STYLE & ~wxRESIZE_BORDER & ~wxMAXIMIZE_BOX),
    settings(_settings)
{
    wxnotebook = new wxNotebook(this, wxID_ANY, wxPoint(APP_MARGIN, APP_MARGIN), wxSize(APP_WIDTH - 23, APP_HEIGHT - 45), wxNB_TOP);

    tab_status = new wxPanel(wxnotebook, wxID_ANY);
    tab_log = new wxPanel(wxnotebook, wxID_ANY);
    tab_settings = new wxPanel(wxnotebook, wxID_ANY);

    wxnotebook->AddPage(tab_status, "Status", true);
    wxnotebook->AddPage(tab_settings, "Settings");
    wxnotebook->AddPage(tab_log, "Log");

    #ifdef _WIN32
        this->SetIcon(icon);
    #endif

    tb = new wxMTaskbar(this);
    tb->SetIcon(icon, "Mousedroid");
    
    InitStatusTab();
    InitSettingsTab();
    InitLogTab();
}   

void wxMain::InitStatusTab()
{    
    wxBoxSizer *topsizer = new wxBoxSizer(wxVERTICAL);

    // ==== This PC info ====
    wxStaticBoxSizer *ipsizer = new wxStaticBoxSizer(wxVERTICAL, tab_status, "This PC");

    wxBoxSizer *r1 = new wxBoxSizer(wxHORIZONTAL);

    r1->Add(new wxStaticText(tab_status, wxID_ANY, "Name: "));
    wxStaticText *sb1 = new wxStaticText(tab_status, HOST_NAME_TXT, "");
    sb1->SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    r1->Add(sb1);

    wxBoxSizer *r2 = new wxBoxSizer(wxHORIZONTAL);
    r2->Add(new wxStaticText(tab_status, wxID_ANY, "Listener: "));
    wxStaticText *sb2 = new wxStaticText(tab_status, HOST_CONNECTION_TXT, "");
    sb2->SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    r2->Add(sb2);

    ipsizer->Add(r1);
    ipsizer->Add(r2);

    // ==== Device list ==== 
    wxdevlist = new wxDeviceList(tab_status, wxID_ANY);
        
    wxStaticBoxSizer *devsizer = new wxStaticBoxSizer(wxVERTICAL, tab_status, "USB devices");
    devsizer->Add(wxdevlist, wxSizerFlags(1).Expand().Border(wxALL, 5));

    wxBoxSizer *serialSizer = new wxBoxSizer(wxHORIZONTAL);
    serialSizer->Add(new wxStaticText(tab_status, wxID_ANY, "ADB serial: "),
                     wxSizerFlags().CenterVertical().Border(wxRIGHT, 5));
    adbDeviceChoice = new wxChoice(tab_status, wxID_ANY);
    adbDeviceChoice->SetMinSize(wxSize(260, -1));
    adbDeviceChoice->Bind(wxEVT_CHOICE, &wxMain::AdbChoiceHandler, this);
    serialSizer->Add(adbDeviceChoice, wxSizerFlags(1).Expand());
    devsizer->Add(serialSizer, wxSizerFlags().Expand().Border(wxALL, 5));

    adbStatus = new wxStaticText(tab_status, wxID_ANY,
        "Waiting for ADB device status… Wi-Fi mode is disabled for safety.",
        wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
    adbStatus->Wrap(APP_WIDTH - 55);
    devsizer->Add(adbStatus, wxSizerFlags().Expand().Border(wxALL, 5));

    topsizer->Add(ipsizer, wxSizerFlags().Expand().Border(wxALL, 5));
    topsizer->Add(devsizer, wxSizerFlags(1).Expand().Border(wxALL, 5));

    tab_status->SetSizerAndFit(topsizer);
}

void wxMain::InitLogTab()
{
    wxBoxSizer *topsizer3 = new wxBoxSizer(wxVERTICAL);
    
    wxListView *monitor = new wxListView(tab_log, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT);

    monitor->AppendColumn("Time");
    monitor->AppendColumn("Message", wxLIST_FORMAT_LEFT, 380);

    Logger::monitor = monitor;

    topsizer3->Add(monitor, wxSizerFlags(1).Expand());

    tab_log->SetSizerAndFit(topsizer3);
}

void wxMain::InitSettingsTab()
{
    wxBoxSizer *topsizer2 = new wxBoxSizer(wxVERTICAL);

    // ==== System settings ====
    wxStaticBoxSizer *sysboxsizer = new wxStaticBoxSizer(wxVERTICAL, tab_settings, "System");

    #ifdef _WIN32
        // Startup checkbox
        wxCheckBox *chk_startup = new wxCheckBox(tab_settings, CHK_STARTUP, "Run at startup");
        chk_startup->SetValue(settings.GetRunAtStartup());
        sysboxsizer->Add(chk_startup, wxSizerFlags().Border(wxTOP, 10));
    #endif

    // Minimize to taskbar checkbox
    wxCheckBox *chk_mintask = new wxCheckBox(tab_settings, CHK_MINTASK, "Minimize to taskbar");
    chk_mintask->SetValue(settings.GetMinToTaskbar());
    sysboxsizer->Add(chk_mintask, wxSizerFlags().Border(wxTOP | wxBOTTOM, 5));

    // ==== Controls settings ====
    wxStaticBoxSizer *ctrlboxsizer = new wxStaticBoxSizer(wxVERTICAL, tab_settings, "Controls");

    // Move sensitivity slider
    wxBoxSizer *b1 = new wxBoxSizer(wxHORIZONTAL);
    b1->Add(new wxStaticText(tab_settings, wxID_ANY, "Move sensitivity:"), wxSizerFlags().Border(wxTOP, 4));

    wxSlider *ms = new wxSlider(
        tab_settings, SL_MOVE, 10, 1, 20, wxDefaultPosition, wxSize(150, wxDefaultSize.y), 
        #ifdef _WIN32
            wxSL_TICKS 
        #else 
            wxSL_VALUE_LABEL
        #endif
    );

    ms->SetValue(settings.GetMoveSensitivity());
    b1->Add(ms);
    ctrlboxsizer->Add(b1);

    // Scroll sensitivity slider
    wxBoxSizer *b2 = new wxBoxSizer(wxHORIZONTAL);
    b2->Add(new wxStaticText(tab_settings, wxID_ANY, "Scroll sensitivity:"), wxSizerFlags().Border(wxTOP, 4));

    wxSlider *ss = new wxSlider(
        tab_settings, SL_SCROLL, 10, 1, 20, wxDefaultPosition, wxSize(150, wxDefaultSize.y), 
        #ifdef _WIN32
            wxSL_TICKS 
        #else 
            wxSL_VALUE_LABEL
        #endif
    );
    
    ss->SetValue(settings.GetScrollSensitivity());
    b2->Add(ss);
    ctrlboxsizer->Add(b2);

    // ==== Info ====
    wxBoxSizer *info = new wxBoxSizer(wxVERTICAL);

    wxStaticText *t = new wxStaticText(tab_settings, wxID_ANY, "Pocket Pad / Mousedroid v1.6");
    t->SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    info->Add(t, wxSizerFlags().Center());
    info->Add(new wxHyperlinkCtrl(tab_settings, wxID_ANY, "Github", "https://github.com/darusc/Mousedroid"), wxSizerFlags().Center());


    topsizer2->Add(sysboxsizer, wxSizerFlags().Expand().Border(wxALL, 5));
    topsizer2->Add(ctrlboxsizer, wxSizerFlags().Expand().Border(wxALL, 5));
    topsizer2->Add(info, wxSizerFlags().Expand().Border(wxTOP, 150));

    tab_settings->SetSizerAndFit(topsizer2);
}

void wxMain::SetHostInfo(std::string hostname, std::string listener)
{
    if (auto* name = dynamic_cast<wxStaticText*>(FindWindowById(HOST_NAME_TXT))) {
        name->SetLabelText(wxString::FromUTF8(hostname.c_str()));
    }
    if (auto* endpoint = dynamic_cast<wxStaticText*>(FindWindowById(HOST_CONNECTION_TXT))) {
        endpoint->SetLabelText(wxString::FromUTF8(listener.c_str()));
    }
}

void wxMain::SetAdbState(const PocketPad::Adb::Snapshot& snapshot)
{
    if (adbStatus != nullptr) {
        std::string message = snapshot.message.empty()
            ? PocketPad::Adb::Coordinator::StatusText(snapshot.state)
            : snapshot.message;
        if (snapshot.state == PocketPad::Adb::DeviceState::Ready && !snapshot.selectedSerial.empty()) {
            message += " (" + snapshot.selectedSerial + ")";
        }
        message += "\nWi-Fi mode is disabled until authenticated pairing is implemented.";
        adbStatus->SetLabelText(wxString::FromUTF8(message.c_str()));
        adbStatus->Wrap(APP_WIDTH - 55);
    }

    updatingAdbChoices = true;
    adbDeviceChoice->Clear();
    adbChoiceSerials.clear();
    adbDeviceChoice->Append("Automatic / choose a device");
    adbChoiceSerials.emplace_back();
    for (const auto& device : snapshot.devices) {
        const std::string label = device.serial + " (" + device.state + ")";
        adbDeviceChoice->Append(wxString::FromUTF8(label.c_str()));
        adbChoiceSerials.push_back(device.serial);
    }

    std::string toSelect = preferredAdbSerial;
    if (toSelect.empty() && snapshot.state == PocketPad::Adb::DeviceState::Ready) {
        toSelect = snapshot.selectedSerial;
    }
    auto found = std::find(adbChoiceSerials.begin() + 1, adbChoiceSerials.end(), toSelect);
    if (!toSelect.empty() && found == adbChoiceSerials.end()) {
        const std::string label = toSelect + " (not detected)";
        adbDeviceChoice->Append(wxString::FromUTF8(label.c_str()));
        adbChoiceSerials.push_back(toSelect);
        found = adbChoiceSerials.end() - 1;
    }
    if (found != adbChoiceSerials.end()) {
        adbDeviceChoice->SetSelection(static_cast<int>(std::distance(adbChoiceSerials.begin(), found)));
    } else {
        adbDeviceChoice->SetSelection(0);
    }
    updatingAdbChoices = false;
}

void wxMain::SetAdbSelectionHandler(std::function<void(std::string)> handler)
{
    adbSelectionHandler = std::move(handler);
}

void wxMain::AdbChoiceHandler(wxCommandEvent&)
{
    if (updatingAdbChoices || adbDeviceChoice == nullptr) return;
    const int selection = adbDeviceChoice->GetSelection();
    if (selection < 0 || static_cast<size_t>(selection) >= adbChoiceSerials.size()) return;
    preferredAdbSerial = adbChoiceSerials[static_cast<size_t>(selection)];
    if (adbSelectionHandler) adbSelectionHandler(preferredAdbSerial);
}

void wxMain::HideWindow(wxIconizeEvent &evt)
{
    if(!settings.GetMinToTaskbar())
    {
        this->Hide();
        evt.Skip();
    }
}

void wxMain::CheckboxCmdHandler(wxCommandEvent &evt)
{
    if(evt.GetId() == CHK_STARTUP)
        settings.SetRunAtStartup(evt.IsChecked());
    else
        settings.SetMinimizeToTaskbar(evt.IsChecked());
}

void wxMain::SliderCmdHandler(wxScrollEvent &evt)
{
    if(evt.GetId() == SL_MOVE)
        settings.SetMoveSensitivity(evt.GetPosition());
    else
        settings.SetScrollSensitivity(evt.GetPosition());
}

wxMain::~wxMain()
{
    Logger::monitor = nullptr;
    delete tb;
}