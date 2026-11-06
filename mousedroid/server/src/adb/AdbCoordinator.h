#pragma once

#include <chrono>
#include <functional>
#include <string>
#include <vector>

namespace PocketPad::Adb {

enum class DeviceState {
    NoDevice,
    Unauthorized,
    Offline,
    Ready,
    MultipleDevices,
    SelectedDeviceUnavailable,
    AdbUnavailable,
    ReverseUnavailable,
    ReverseConflict
};

struct Device {
    std::string serial;
    std::string state;
    std::string model;

    bool operator==(const Device& other) const {
        return serial == other.serial && state == other.state && model == other.model;
    }
};

struct CommandResult {
    bool started = true;
    bool timedOut = false;
    int exitCode = 0;
    std::string output;

    bool Succeeded() const { return started && !timedOut && exitCode == 0; }
};

struct Snapshot {
    DeviceState state = DeviceState::NoDevice;
    std::vector<Device> devices;
    std::string selectedSerial;
    std::string message;

    bool operator==(const Snapshot& other) const {
        return state == other.state && devices == other.devices &&
            selectedSerial == other.selectedSerial && message == other.message;
    }
    bool operator!=(const Snapshot& other) const { return !(*this == other); }
};

using CommandRunner = std::function<CommandResult(
    const std::vector<std::string>& arguments,
    std::chrono::milliseconds timeout)>;

/**
 * Deterministic ADB device/reverse state coordinator. Process creation is
 * injected so production uses the bundled, argument-safe runner and tests use
 * a fake without requiring a device or starting/stopping the shared ADB server.
 */
class Coordinator {
public:
    explicit Coordinator(CommandRunner runner, int port = 6969);

    Snapshot Poll(const std::string& preferredSerial = "") const;

    static std::vector<Device> ParseDevicesOutput(const std::string& output);
    static std::string StatusText(DeviceState state);

private:
    enum class ReverseMapping { Missing, Match, Conflict };

    CommandRunner runner_;
    int port_;

    ReverseMapping InspectReverseList(const std::string& output,
                                     const std::string& serial) const;
    std::string PortSpec() const;
};

}  // namespace PocketPad::Adb
