#pragma once

#include "adb/AdbCoordinator.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

namespace PocketPad::Adb {

class Monitor {
public:
    using StateCallback = std::function<void(const Snapshot&)>;

    explicit Monitor(StateCallback callback,
                     std::chrono::milliseconds pollInterval = std::chrono::milliseconds(1500),
                     CommandRunner runner = {});
    ~Monitor();

    Monitor(const Monitor&) = delete;
    Monitor& operator=(const Monitor&) = delete;

    void Start();
    void Stop();
    void SetPreferredSerial(std::string serial);

private:
    void Run();

    StateCallback callback_;
    std::chrono::milliseconds pollInterval_;
    CommandRunner runner_;
    std::mutex mutex_;
    std::condition_variable wake_;
    std::thread thread_;
    bool stopping_ = false;
    bool started_ = false;
    bool preferenceChanged_ = false;
    std::string preferredSerial_;
};

}  // namespace PocketPad::Adb
