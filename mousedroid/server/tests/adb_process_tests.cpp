#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "adb/AdbProcess.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

std::wstring CurrentExecutablePath() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                 static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size() - 1) return std::wstring(buffer.data(), length);
        if (buffer.size() >= 32768) return {};
        buffer.resize(buffer.size() * 2);
    }
}

bool Check(bool condition, const char* message) {
    if (condition) return true;
    std::cerr << "FAIL: " << message << '\n';
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    const std::filesystem::path launchedAs = std::filesystem::path(argv[0]).filename();
    if (launchedAs == L"fake adb.exe") {
        if (argc > 1 && std::string(argv[1]) == "--sleep") {
            Sleep(5000);
            return 0;
        }
        for (int i = 1; i < argc; ++i) std::cout << argv[i] << '\n';
        return 0;
    }

    const std::wstring originalExe = CurrentExecutablePath();
    if (!Check(!originalExe.empty(), "could not locate process test executable")) return 1;

    wchar_t tempPath[MAX_PATH]{};
    const DWORD tempLength = GetTempPathW(MAX_PATH, tempPath);
    if (!Check(tempLength > 0 && tempLength < MAX_PATH, "could not locate temp directory")) return 1;
    const std::filesystem::path fakeDirectory = std::filesystem::path(tempPath) /
        (L"Pocket Pad ADB test " + std::to_wstring(GetCurrentProcessId()));
    std::error_code filesystemError;
    std::filesystem::create_directories(fakeDirectory, filesystemError);
    if (!Check(!filesystemError, "could not create spaced fake-ADB directory")) return 1;
    const std::filesystem::path fakeExe = fakeDirectory / L"fake adb.exe";
    if (!Check(CopyFileW(originalExe.c_str(), fakeExe.c_str(), FALSE) != 0,
               "could not copy fake ADB executable")) {
        std::filesystem::remove_all(fakeDirectory, filesystemError);
        return 1;
    }

    bool passed = true;
    const std::vector<std::string> expected = {
        "devices", "-l", "serial with spaces", "serial&must-not-run",
        "trailing path\\\\", "quote\"inside"
    };
    const PocketPad::Adb::CommandResult result = PocketPad::Adb::RunAdbProcess(
        fakeExe.wstring(), expected, std::chrono::seconds(5));
    std::istringstream output(result.output);
    std::vector<std::string> actual;
    std::string line;
    while (std::getline(output, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        actual.push_back(line);
    }
    passed &= Check(result.Succeeded(), "direct process launch with a spaced path failed");
    passed &= Check(actual == expected, "Windows argument boundaries/quoting were not preserved");

    const auto timedOut = PocketPad::Adb::RunAdbProcess(
        fakeExe.wstring(), {"--sleep"}, std::chrono::milliseconds(100));
    passed &= Check(timedOut.started && timedOut.timedOut,
                    "bounded process timeout did not terminate the child process");

    std::filesystem::remove_all(fakeDirectory, filesystemError);
    if (!passed) return 1;
    std::cout << "PASS: Windows direct process launch, spaced executable path, exact argument preservation, and timeout\n";
    return 0;
}

#endif  // _WIN32
