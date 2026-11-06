#include "logger.h"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <sstream>

namespace {

std::mutex outputMutex;
std::ofstream fileOutput("app.log", std::ios::app);

std::string CurrentTime() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &now);
#else
    localtime_r(&now, &localTime);
#endif
    std::ostringstream formatted;
    formatted << std::put_time(&localTime, "%H:%M:%S");
    return formatted.str();
}

}  // namespace

wxListView* Logger::monitor = nullptr;

void Logger::displayOutput(const std::string& message) {
    const std::string time = CurrentTime();
    {
        std::lock_guard<std::mutex> lock(outputMutex);
        if (fileOutput.is_open()) fileOutput << time << " " << message << std::endl;
    }

    wxApp* app = wxTheApp;
    if (app == nullptr) return;
    app->CallAfter([message, time] {
        wxListView* list = Logger::monitor;
        if (list == nullptr || list->IsBeingDeleted()) return;
        const long row = list->InsertItem(list->GetItemCount(), time);
        list->SetItem(row, 1, message);
        list->EnsureVisible(row);
    });
}
