#include "adb/AdbMonitor.h"

#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <vector>

using PocketPad::Adb::CommandResult;
using PocketPad::Adb::DeviceState;
using PocketPad::Adb::Monitor;
using PocketPad::Adb::Snapshot;

int main() {
    std::mutex stateMutex;
    std::condition_variable stateChanged;
    std::vector<Snapshot> snapshots;
    std::vector<std::vector<std::string>> calls;
    std::string reverseOutput;
    const std::thread::id testThread = std::this_thread::get_id();
    std::thread::id callbackThread;

    auto runner = [&](const std::vector<std::string>& args, std::chrono::milliseconds) {
        std::lock_guard<std::mutex> lock(stateMutex);
        calls.push_back(args);
        CommandResult result;
        if (args == std::vector<std::string>({"devices", "-l"})) {
            result.output = "List of devices attached\nphone-a device\nphone-b device\n";
        } else if (args.size() == 4 && args[0] == "-s" && args[2] == "reverse" && args[3] == "--list") {
            result.output = reverseOutput;
        } else if (args.size() == 5 && args[0] == "-s" && args[2] == "reverse") {
            reverseOutput = args[1] + " " + args[3] + " " + args[4] + "\n";
        } else {
            result.started = false;
            result.exitCode = -1;
            result.output = "unexpected fake command";
        }
        return result;
    };

    Monitor monitor([&](const Snapshot& snapshot) {
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            callbackThread = std::this_thread::get_id();
            snapshots.push_back(snapshot);
        }
        stateChanged.notify_all();
    }, std::chrono::hours(1), runner);

    monitor.Start();
    {
        std::unique_lock<std::mutex> lock(stateMutex);
        if (!stateChanged.wait_for(lock, std::chrono::seconds(3), [&] { return !snapshots.empty(); })) {
            std::cerr << "FAIL: monitor did not publish its initial snapshot\n";
            monitor.Stop();
            return 1;
        }
        if (snapshots.back().state != DeviceState::MultipleDevices) {
            std::cerr << "FAIL: expected explicit multi-device selection state\n";
            lock.unlock();
            monitor.Stop();
            return 1;
        }
        if (callbackThread == testThread) {
            std::cerr << "FAIL: ADB state callback ran on the caller/UI thread\n";
            lock.unlock();
            monitor.Stop();
            return 1;
        }
    }

    monitor.SetPreferredSerial("phone-b");
    {
        std::unique_lock<std::mutex> lock(stateMutex);
        const bool updated = stateChanged.wait_for(lock, std::chrono::seconds(3), [&] {
            return !snapshots.empty() && snapshots.back().state == DeviceState::Ready &&
                   snapshots.back().selectedSerial == "phone-b";
        });
        if (!updated) {
            std::cerr << "FAIL: serial selection did not wake the background monitor\n";
            lock.unlock();
            monitor.Stop();
            return 1;
        }
        bool targetedPhoneB = false;
        for (const auto& call : calls) {
            if (call.size() == 5 && call[2] == "reverse" && call[1] == "phone-b") {
                targetedPhoneB = true;
            }
        }
        if (!targetedPhoneB) {
            std::cerr << "FAIL: selected serial did not receive the reverse rule\n";
            lock.unlock();
            monitor.Stop();
            return 1;
        }
    }

    monitor.Stop();
    std::cout << "PASS: asynchronous monitor, explicit serial wake-up, and clean stop\n";
    return 0;
}
