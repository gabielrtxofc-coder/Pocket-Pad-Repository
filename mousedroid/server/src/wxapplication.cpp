#include "wxapplication.h"

#include <utility>

wxIMPLEMENT_APP(wxApplication);

wxApplication::wxApplication() = default;

wxApplication::~wxApplication() {
    if (server != nullptr) {
        server->Close();
        delete server;
        server = nullptr;
    }
}

bool wxApplication::OnInit() {
    main_frame = new wxMain(settings);
    server = new Server(6969, *this, inputManager);

    const Server::HostInfo hostInfo = server->GetHostInfo();
    main_frame->SetHostInfo(std::get<0>(hostInfo), std::get<1>(hostInfo));
    main_frame->SetAdbSelectionHandler([this](std::string serial) {
        if (server != nullptr) server->SetPreferredAdbSerial(std::move(serial));
    });
    main_frame->Bind(wxEVT_CLOSE_WINDOW, &wxApplication::OnWindowCloseEvent, this);

    if (!settings.GetRunAtStartup()) main_frame->Show();

    server->Start();
    return true;
}

int wxApplication::OnExit() {
    Logger::monitor = nullptr;
    main_frame = nullptr;
    if (server != nullptr) {
        server->Close();
        delete server;
        server = nullptr;
    }
    return wxApp::OnExit();
}

void wxApplication::OnDeviceConnected(std::string) {
    CallAfter([this] { UpdateDeviceList(); });
}

void wxApplication::OnDeviceDisconnected(std::string) {
    CallAfter([this] { UpdateDeviceList(); });
}

void wxApplication::OnAdbStateChanged(const PocketPad::Adb::Snapshot& state) {
    CallAfter([this, state] {
        if (main_frame != nullptr && !main_frame->IsBeingDeleted()) {
            main_frame->SetAdbState(state);
        }
    });
}

void wxApplication::UpdateDeviceList() {
    if (server == nullptr || main_frame == nullptr || main_frame->IsBeingDeleted()) return;
    main_frame->wxdevlist->SetDevices(server->GetConnectedDevicesInfo());
}

void wxApplication::OnWindowCloseEvent(wxCloseEvent& evt) {
    if (main_frame == nullptr) {
        evt.Skip();
        return;
    }

    wxMessageDialog box(main_frame,
        "This will disconnect all connected devices. Proceed?",
        "Confirm", wxYES_NO | wxICON_INFORMATION);
    if (box.ShowModal() != wxID_YES) {
        evt.Veto();
        return;
    }

    wxMain* closingFrame = main_frame;
    closingFrame->Hide();
    if (server != nullptr) server->Close();
    Logger::monitor = nullptr;
    main_frame = nullptr;
    closingFrame->Destroy();
}
