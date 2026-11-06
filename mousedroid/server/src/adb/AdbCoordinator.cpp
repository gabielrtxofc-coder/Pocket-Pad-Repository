#include "adb/AdbCoordinator.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <utility>

namespace PocketPad::Adb {
namespace {

std::string Trim(const std::string& value) {
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char c) {
        return std::isspace(c) != 0;
    });
    if (first == value.end()) return {};
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char c) {
        return std::isspace(c) != 0;
    }).base();
    return std::string(first, last);
}

std::vector<std::string> SplitWhitespace(const std::string& line) {
    std::istringstream stream(line);
    std::vector<std::string> tokens;
    std::string token;
    while (stream >> token) tokens.push_back(token);
    return tokens;
}

std::string ExtractFailure(const CommandResult& result, const std::string& fallback) {
    const std::string output = Trim(result.output);
    if (result.timedOut) return fallback + " (ADB command timed out).";
    if (!result.started) return fallback + " (bundled ADB could not be started).";
    if (!output.empty()) return fallback + ": " + output;
    return fallback;
}

}  // namespace

Coordinator::Coordinator(CommandRunner runner, int port)
    : runner_(std::move(runner)), port_(port) {}

std::string Coordinator::PortSpec() const {
    return "tcp:" + std::to_string(port_);
}

std::vector<Device> Coordinator::ParseDevicesOutput(const std::string& output) {
    std::vector<Device> devices;
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        const std::vector<std::string> tokens = SplitWhitespace(line);
        if (tokens.size() < 2 || tokens[0] == "List" || tokens[0].front() == '*') continue;

        Device device;
        device.serial = tokens[0];
        device.state = tokens[1];
        for (size_t i = 2; i < tokens.size(); ++i) {
            const std::string prefix = "model:";
            if (tokens[i].compare(0, prefix.size(), prefix) == 0) {
                device.model = tokens[i].substr(prefix.size());
                break;
            }
        }
        devices.push_back(std::move(device));
    }
    return devices;
}

Coordinator::ReverseMapping Coordinator::InspectReverseList(
    const std::string& output, const std::string& serial) const {
    const std::string localPort = PortSpec();
    std::istringstream lines(output);
    std::string line;
    while (std::getline(lines, line)) {
        const std::vector<std::string> tokens = SplitWhitespace(line);
        if (tokens.size() < 2) continue;

        size_t localIndex = 0;
        size_t remoteIndex = 1;
        if (tokens.size() >= 3 && tokens[1].compare(0, 4, "tcp:") == 0) {
            // `adb reverse --list` can include the serial (including network
            // serials containing ':'), while a serial-scoped listing may omit it.
            if (tokens[0] != serial) continue;
            localIndex = 1;
            remoteIndex = 2;
        }
        if (remoteIndex >= tokens.size() || tokens[localIndex] != localPort) continue;
        return tokens[remoteIndex] == localPort ? ReverseMapping::Match
                                                : ReverseMapping::Conflict;
    }
    return ReverseMapping::Missing;
}

Snapshot Coordinator::Poll(const std::string& preferredSerial) const {
    Snapshot snapshot;
    const auto devicesResult = runner_({"devices", "-l"}, std::chrono::milliseconds(1500));
    if (!devicesResult.Succeeded()) {
        snapshot.state = DeviceState::AdbUnavailable;
        snapshot.message = ExtractFailure(devicesResult, StatusText(snapshot.state));
        return snapshot;
    }

    snapshot.devices = ParseDevicesOutput(devicesResult.output);
    if (snapshot.devices.empty()) {
        snapshot.state = DeviceState::NoDevice;
        snapshot.message = StatusText(snapshot.state);
        return snapshot;
    }

    const Device* selected = nullptr;
    if (!preferredSerial.empty()) {
        const auto found = std::find_if(snapshot.devices.begin(), snapshot.devices.end(),
            [&preferredSerial](const Device& device) { return device.serial == preferredSerial; });
        if (found == snapshot.devices.end()) {
            snapshot.state = DeviceState::SelectedDeviceUnavailable;
            snapshot.message = StatusText(snapshot.state);
            return snapshot;
        }
        selected = &*found;
        if (selected->state != "device") {
            snapshot.state = selected->state == "unauthorized"
                ? DeviceState::Unauthorized : DeviceState::Offline;
            snapshot.message = StatusText(snapshot.state);
            return snapshot;
        }
    } else {
        std::vector<const Device*> online;
        for (const Device& device : snapshot.devices) {
            if (device.state == "device") online.push_back(&device);
        }
        if (online.size() > 1) {
            snapshot.state = DeviceState::MultipleDevices;
            snapshot.message = StatusText(snapshot.state);
            return snapshot;
        }
        if (online.size() == 1) {
            selected = online.front();
        } else {
            const auto unauthorized = std::find_if(snapshot.devices.begin(), snapshot.devices.end(),
                [](const Device& device) { return device.state == "unauthorized"; });
            snapshot.state = unauthorized != snapshot.devices.end()
                ? DeviceState::Unauthorized : DeviceState::Offline;
            snapshot.message = StatusText(snapshot.state);
            return snapshot;
        }
    }

    snapshot.selectedSerial = selected->serial;
    const auto reverseList = runner_({"-s", selected->serial, "reverse", "--list"},
                                     std::chrono::milliseconds(1500));
    if (!reverseList.Succeeded()) {
        snapshot.state = DeviceState::ReverseUnavailable;
        snapshot.message = ExtractFailure(reverseList,
            "Could not safely inspect the phone's existing reverse rules");
        return snapshot;
    }

    const ReverseMapping mapping = InspectReverseList(reverseList.output, selected->serial);
    if (mapping == ReverseMapping::Match) {
        snapshot.state = DeviceState::Ready;
        snapshot.message = StatusText(snapshot.state);
        return snapshot;
    }
    if (mapping == ReverseMapping::Conflict) {
        snapshot.state = DeviceState::ReverseConflict;
        snapshot.message = StatusText(snapshot.state);
        return snapshot;
    }

    const auto reverse = runner_({"-s", selected->serial, "reverse", PortSpec(), PortSpec()},
                                 std::chrono::milliseconds(1500));
    if (!reverse.Succeeded()) {
        snapshot.state = DeviceState::ReverseUnavailable;
        snapshot.message = ExtractFailure(reverse, StatusText(snapshot.state));
        return snapshot;
    }

    snapshot.state = DeviceState::Ready;
    snapshot.message = StatusText(snapshot.state);
    return snapshot;
}

std::string Coordinator::StatusText(DeviceState state) {
    switch (state) {
        case DeviceState::NoDevice:
            return "No ADB device detected. Connect the phone by USB.";
        case DeviceState::Unauthorized:
            return "USB device is unauthorized. Accept the RSA debugging prompt on the phone.";
        case DeviceState::Offline:
            return "ADB device is offline. Reconnect USB or restart USB debugging.";
        case DeviceState::Ready:
            return "USB device ready; reverse tunnel active.";
        case DeviceState::MultipleDevices:
            return "Multiple authorized devices detected. Select a serial.";
        case DeviceState::SelectedDeviceUnavailable:
            return "The selected serial is not currently connected. Choose an available device.";
        case DeviceState::AdbUnavailable:
            return "Bundled ADB is unavailable.";
        case DeviceState::ReverseUnavailable:
            return "The device is online, but USB reverse forwarding failed.";
        case DeviceState::ReverseConflict:
            return "Port 6969 is already reversed to a different target on this device.";
    }
    return "ADB status is unknown.";
}

}  // namespace PocketPad::Adb
