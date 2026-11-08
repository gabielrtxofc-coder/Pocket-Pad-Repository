#include "adb/AdbCoordinator.h"

#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

using PocketPad::Adb::CommandResult;
using PocketPad::Adb::Coordinator;
using PocketPad::Adb::DeviceState;
using PocketPad::Adb::Snapshot;

namespace {

struct FakeAdb {
    std::string devicesOutput = "List of devices attached\n";
    std::string reverseOutput;
    int reverseApplyExitCode = 0;
    bool reverseListSucceeds = true;
    std::vector<std::vector<std::string>> calls;

    CommandResult Run(const std::vector<std::string>& args) {
        calls.push_back(args);
        CommandResult result;
        if (args.size() == 2 && args[0] == "devices" && args[1] == "-l") {
            result.output = devicesOutput;
            return result;
        }
        if (args.size() == 4 && args[0] == "-s" && args[2] == "reverse" && args[3] == "--list") {
            if (!reverseListSucceeds) {
                result.started = false;
                result.exitCode = -1;
                result.output = "fake reverse-list failure";
            } else {
                result.output = reverseOutput;
            }
            return result;
        }
        if (args.size() == 5 && args[0] == "-s" && args[2] == "reverse") {
            result.exitCode = reverseApplyExitCode;
            if (result.exitCode == 0) {
                // The first argument is -s; use the selected serial in the
                // all-devices output form accepted by the coordinator.
                reverseOutput = args[1] + " " + args[3] + " " + args[4] + "\n";
            }
            return result;
        }
        result.started = false;
        result.exitCode = -1;
        result.output = "unexpected fake ADB arguments";
        return result;
    }

    Coordinator MakeCoordinator() {
        return Coordinator([this](const std::vector<std::string>& args, std::chrono::milliseconds) {
            return Run(args);
        });
    }

    int CountReverseApply() const {
        int count = 0;
        for (const auto& call : calls) {
            if (call.size() == 5 && call[0] == "-s" && call[2] == "reverse") ++count;
        }
        return count;
    }
};

void Expect(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void Scenario(int number, const std::string& name, const std::function<void()>& test) {
    test();
    std::cout << "PASS " << number << ": " << name << '\n';
}

}  // namespace

int main() {
    try {
        Scenario(1, "no device waits without issuing reverse commands", [] {
            FakeAdb fake;
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::NoDevice, "expected no-device state");
            Expect(fake.calls.size() == 1, "no device must not trigger reverse");
        });

        Scenario(2, "unauthorized device is distinct and requires RSA approval", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a unauthorized usb:1-1\n";
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::Unauthorized, "expected unauthorized state");
            Expect(fake.CountReverseApply() == 0, "unauthorized device must not get a reverse rule");
        });

        Scenario(3, "offline device is reported separately", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a offline usb:1-1\n";
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::Offline, "expected offline state");
        });

        Scenario(4, "single authorized device receives a serial-scoped reverse rule", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device product:a model:Galaxy_A12\n";
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::Ready, "expected ready state");
            Expect(snapshot.selectedSerial == "phone-a", "wrong selected serial");
            Expect(fake.calls.back() == std::vector<std::string>({"-s", "phone-a", "reverse", "tcp:6969", "tcp:6969"}),
                   "reverse command must target the selected serial");
        });

        Scenario(5, "existing reverse mapping is reused", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            fake.reverseOutput = "phone-a tcp:6969 tcp:6969\n";
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::Ready, "expected ready state");
            Expect(fake.CountReverseApply() == 0, "existing mapping should not be rewritten");
        });

        Scenario(6, "lost reverse mapping after ADB-server restart is restored", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            fake.reverseOutput = ""; // ADB daemon restarted and forgot the mapping.
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::Ready, "expected ready after restoring mapping");
            Expect(fake.CountReverseApply() == 1, "mapping should be re-applied");
        });

        Scenario(7, "USB loss followed by reappearance is recovered", [] {
            FakeAdb fake;
            Coordinator coordinator = fake.MakeCoordinator();
            const auto detached = coordinator.Poll();
            Expect(detached.state == DeviceState::NoDevice, "expected detached state");
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            const auto reattached = coordinator.Poll();
            Expect(reattached.state == DeviceState::Ready, "expected ready after USB returns");
            Expect(fake.CountReverseApply() == 1, "reverse should be restored after reconnection");
        });

        Scenario(8, "a restarted Windows server instance restores its serial reverse", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            Coordinator restartedServer = fake.MakeCoordinator();
            const auto snapshot = restartedServer.Poll();
            Expect(snapshot.state == DeviceState::Ready, "expected ready after server restart");
            Expect(fake.CountReverseApply() == 1, "new process must ensure the reverse mapping");
        });

        Scenario(9, "multiple authorized devices require explicit selection", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\nphone-b device\n";
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::MultipleDevices, "expected explicit selection state");
            Expect(fake.CountReverseApply() == 0, "ambiguous devices must not be selected implicitly");
        });

        Scenario(10, "selected serial is the only device receiving reverse", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\nphone-b device\n";
            const auto snapshot = fake.MakeCoordinator().Poll("phone-b");
            Expect(snapshot.state == DeviceState::Ready, "expected selected device ready");
            Expect(snapshot.selectedSerial == "phone-b", "selected serial was not retained");
            Expect(fake.calls.back()[1] == "phone-b", "reverse targeted the wrong phone");
        });

        Scenario(11, "selected unauthorized serial is not replaced by another online phone", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\nphone-b unauthorized\n";
            const auto snapshot = fake.MakeCoordinator().Poll("phone-b");
            Expect(snapshot.state == DeviceState::Unauthorized, "expected selected phone unauthorized");
            Expect(fake.CountReverseApply() == 0, "must not fall back to a different serial");
        });

        Scenario(12, "missing selected serial is not silently switched", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            const auto snapshot = fake.MakeCoordinator().Poll("phone-b");
            Expect(snapshot.state == DeviceState::SelectedDeviceUnavailable, "expected selected-device-missing state");
            Expect(fake.CountReverseApply() == 0, "must not forward a different serial");
        });

        Scenario(13, "RSA approval transition from unauthorized to device enables forwarding", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a unauthorized\n";
            Coordinator coordinator = fake.MakeCoordinator();
            Expect(coordinator.Poll().state == DeviceState::Unauthorized, "expected RSA authorization wait");
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            Expect(coordinator.Poll().state == DeviceState::Ready, "expected ready only after adb reports device");
            Expect(fake.CountReverseApply() == 1, "authorized transition should set reverse");
        });

        Scenario(14, "reverse command failure is surfaced without a global server shutdown", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            fake.reverseApplyExitCode = 1;
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::ReverseUnavailable, "expected forward failure state");
            for (const auto& call : fake.calls) {
                Expect(std::find(call.begin(), call.end(), "kill-server") == call.end(),
                       "must never kill the shared ADB server");
                Expect(std::find(call.begin(), call.end(), "--remove-all") == call.end(),
                       "must never remove all reverse rules");
            }
        });

        Scenario(15, "a conflicting reverse target is reported and never overwritten", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            fake.reverseOutput = "phone-a tcp:6969 tcp:1234\n";
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::ReverseConflict, "expected reverse conflict state");
            Expect(fake.CountReverseApply() == 0, "must not overwrite another local reverse target");
        });

        Scenario(16, "other ADB client/device remains untouched and shell metacharacters stay one argument", [] {
            FakeAdb fake;
            const std::string selected = "phone&unrelated-client";
            fake.devicesOutput = "List of devices attached\nphone&unrelated-client device\nother-phone device\n";
            const auto snapshot = fake.MakeCoordinator().Poll(selected);
            Expect(snapshot.state == DeviceState::Ready, "expected selected device ready");
            Expect(fake.calls.back()[1] == selected, "serial argument was split or altered");
            for (const auto& call : fake.calls) {
                Expect(std::find(call.begin(), call.end(), "kill-server") == call.end(),
                       "must leave the shared ADB daemon running");
                if (call.size() == 5 && call[2] == "reverse") {
                    Expect(call[1] == selected, "another device was targeted");
                }
            }
        });

        Scenario(17, "failed reverse inspection does not overwrite an unknown mapping", [] {
            FakeAdb fake;
            fake.devicesOutput = "List of devices attached\nphone-a device\n";
            fake.reverseListSucceeds = false;
            const auto snapshot = fake.MakeCoordinator().Poll();
            Expect(snapshot.state == DeviceState::ReverseUnavailable,
                   "expected safe failure when reverse rules cannot be inspected");
            Expect(fake.CountReverseApply() == 0,
                   "must not overwrite a mapping when its current target is unknown");
        });
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
