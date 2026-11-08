#include "adb/AdbMonitor.h"

#ifdef _WIN32
#include "adb/AdbProcess.h"
#endif

#include <exception>
#include <string>
#include <utility>

namespace PocketPad::Adb {

Monitor::Monitor(StateCallback callback, std::chrono::milliseconds pollInterval, CommandRunner runner)
    : callback_(std::move(callback)),
      pollInterval_(pollInterval),
      runner_(std::move(runner)) {}

Monitor::~Monitor() {
    Stop();
}

void Monitor::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (started_) return;
    stopping_ = false;
    started_ = true;
    thread_ = std::thread(&Monitor::Run, this);
}

void Monitor::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!started_) return;
        stopping_ = true;
    }
    wake_.notify_all();
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    started_ = false;
}

void Monitor::SetPreferredSerial(std::string serial) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (preferredSerial_ == serial) return;
        preferredSerial_ = std::move(serial);
        preferenceChanged_ = true;
    }
    wake_.notify_all();
}

void Monitor::Run() {
#ifdef _WIN32
    if (!runner_) {
        const std::wstring adbPath = GetBundledAdbPath();
        runner_ = [adbPath](const std::vector<std::string>& arguments,
                            std::chrono::milliseconds timeout) {
            return RunAdbProcess(adbPath, arguments, timeout);
        };
    }
#else
    if (!runner_) {
        runner_ = [](const std::vector<std::string>&, std::chrono::milliseconds) {
            CommandResult result;
            result.started = false;
            result.exitCode = -1;
            result.output = "Bundled ADB monitoring is supported by the Windows package only.";
            return result;
        };
    }
#endif
    Coordinator coordinator(runner_);

    Snapshot lastSnapshot;
    bool hasSnapshot = false;
    for (;;) {
        std::string preferred;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            if (stopping_) break;
            preferred = preferredSerial_;
            preferenceChanged_ = false;
        }

        Snapshot current;
        try {
            current = coordinator.Poll(preferred);
        } catch (const std::exception& error) {
            current.state = DeviceState::AdbUnavailable;
            current.message = std::string("ADB monitor error: ") + error.what();
        } catch (...) {
            current.state = DeviceState::AdbUnavailable;
            current.message = "ADB monitor error.";
        }
        if (!hasSnapshot || current != lastSnapshot) {
            lastSnapshot = current;
            hasSnapshot = true;
            try {
                if (callback_) callback_(current);
            } catch (...) {
                // UI callbacks must not terminate the monitor thread.
            }
        }

        std::unique_lock<std::mutex> lock(mutex_);
        wake_.wait_for(lock, pollInterval_, [this] {
            return stopping_ || preferenceChanged_;
        });
        if (stopping_) break;
    }
}

}  // namespace PocketPad::Adb
