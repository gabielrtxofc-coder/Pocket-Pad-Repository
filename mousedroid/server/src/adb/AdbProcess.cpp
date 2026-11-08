#ifdef _WIN32

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "adb/AdbProcess.h"

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

namespace PocketPad::Adb {
namespace {

constexpr size_t kMaxCapturedOutput = 64 * 1024;

std::wstring QuoteWindowsArgument(const std::wstring& argument) {
    if (!argument.empty() && argument.find_first_of(L" \t\n\v\"") == std::wstring::npos) {
        return argument;
    }

    std::wstring quoted(1, L'"');
    size_t backslashes = 0;
    for (wchar_t character : argument) {
        if (character == L'\\') {
            ++backslashes;
            continue;
        }
        if (character == L'"') {
            quoted.append(backslashes * 2 + 1, L'\\');
            quoted.push_back(L'"');
            backslashes = 0;
            continue;
        }
        quoted.append(backslashes, L'\\');
        backslashes = 0;
        quoted.push_back(character);
    }
    // Backslashes immediately before the closing quote must be doubled for
    // the Windows CRT command-line parser.
    quoted.append(backslashes * 2, L'\\');
    quoted.push_back(L'"');
    return quoted;
}

bool Utf8ToWide(const std::string& input, std::wstring& output) {
    if (input.empty()) {
        output.clear();
        return true;
    }
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                         input.data(), static_cast<int>(input.size()),
                                         nullptr, 0);
    if (size <= 0) return false;
    output.resize(static_cast<size_t>(size));
    return MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                               input.data(), static_cast<int>(input.size()),
                               output.data(), size) == size;
}

std::wstring CurrentExecutablePath() {
    std::vector<wchar_t> buffer(512);
    for (;;) {
        const DWORD length = GetModuleFileNameW(nullptr, buffer.data(),
                                                 static_cast<DWORD>(buffer.size()));
        if (length == 0) return {};
        if (length < buffer.size() - 1) return std::wstring(buffer.data(), length);
        if (buffer.size() >= 32768) return {};
        buffer.resize(std::min<size_t>(buffer.size() * 2, 32768));
    }
}

void AppendAvailableOutput(HANDLE pipe, std::string& output) {
    for (;;) {
        DWORD available = 0;
        if (!PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) || available == 0) return;

        char buffer[4096];
        const DWORD requested = std::min<DWORD>(available, sizeof(buffer));
        DWORD bytesRead = 0;
        if (!ReadFile(pipe, buffer, requested, &bytesRead, nullptr) || bytesRead == 0) return;
        if (output.size() < kMaxCapturedOutput) {
            const size_t capture = std::min<size_t>(bytesRead, kMaxCapturedOutput - output.size());
            output.append(buffer, capture);
        }
        // Continue draining even after the capture limit so a noisy child
        // cannot block on a full pipe.
    }
}

}  // namespace

std::wstring GetBundledAdbPath() {
    const std::wstring executable = CurrentExecutablePath();
    if (executable.empty()) return {};
    return (std::filesystem::path(executable).parent_path() / L"adb.exe").wstring();
}

CommandResult RunAdbProcess(const std::wstring& executable,
                            const std::vector<std::string>& arguments,
                            std::chrono::milliseconds timeout) {
    CommandResult result;
    if (executable.empty()) {
        result.started = false;
        result.exitCode = -1;
        return result;
    }

    std::wstring commandLine = QuoteWindowsArgument(executable);
    for (const std::string& argument : arguments) {
        std::wstring wideArgument;
        if (!Utf8ToWide(argument, wideArgument)) {
            result.started = false;
            result.exitCode = -1;
            result.output = "ADB argument is not valid UTF-8.";
            return result;
        }
        commandLine.push_back(L' ');
        commandLine.append(QuoteWindowsArgument(wideArgument));
    }

    SECURITY_ATTRIBUTES securityAttributes{};
    securityAttributes.nLength = sizeof(securityAttributes);
    securityAttributes.bInheritHandle = TRUE;

    HANDLE outputRead = nullptr;
    HANDLE outputWrite = nullptr;
    if (!CreatePipe(&outputRead, &outputWrite, &securityAttributes, 0)) {
        result.started = false;
        result.exitCode = static_cast<int>(GetLastError());
        return result;
    }
    if (!SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0)) {
        const DWORD error = GetLastError();
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        result.started = false;
        result.exitCode = static_cast<int>(error);
        return result;
    }

    HANDLE nullInput = CreateFileW(L"NUL", GENERIC_READ,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   &securityAttributes, OPEN_EXISTING,
                                   FILE_ATTRIBUTE_NORMAL, nullptr);
    if (nullInput == INVALID_HANDLE_VALUE) {
        const DWORD error = GetLastError();
        CloseHandle(outputRead);
        CloseHandle(outputWrite);
        result.started = false;
        result.exitCode = static_cast<int>(error);
        return result;
    }

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startupInfo.wShowWindow = SW_HIDE;
    startupInfo.hStdInput = nullInput;
    startupInfo.hStdOutput = outputWrite;
    startupInfo.hStdError = outputWrite;

    PROCESS_INFORMATION processInfo{};
    std::vector<wchar_t> mutableCommandLine(commandLine.begin(), commandLine.end());
    mutableCommandLine.push_back(L'\0');
    const std::filesystem::path workingDirectory = std::filesystem::path(executable).parent_path();
    const BOOL created = CreateProcessW(executable.c_str(), mutableCommandLine.data(),
                                        nullptr, nullptr, TRUE, CREATE_NO_WINDOW,
                                        nullptr, workingDirectory.c_str(),
                                        &startupInfo, &processInfo);
    const DWORD createError = created ? ERROR_SUCCESS : GetLastError();
    CloseHandle(outputWrite);
    CloseHandle(nullInput);

    if (!created) {
        CloseHandle(outputRead);
        result.started = false;
        result.exitCode = static_cast<int>(createError);
        return result;
    }
    CloseHandle(processInfo.hThread);

    const auto deadline = std::chrono::steady_clock::now() + timeout;
    for (;;) {
        AppendAvailableOutput(outputRead, result.output);

        const DWORD waitResult = WaitForSingleObject(processInfo.hProcess, 25);
        if (waitResult == WAIT_OBJECT_0) break;
        if (waitResult == WAIT_FAILED) {
            const DWORD error = GetLastError();
            TerminateProcess(processInfo.hProcess, ERROR_PROCESS_ABORTED);
            WaitForSingleObject(processInfo.hProcess, 1000);
            result.started = false;
            result.exitCode = static_cast<int>(error);
            break;
        }
        if (std::chrono::steady_clock::now() >= deadline) {
            result.timedOut = true;
            TerminateProcess(processInfo.hProcess, ERROR_TIMEOUT);
            WaitForSingleObject(processInfo.hProcess, 1000);
            break;
        }
    }

    AppendAvailableOutput(outputRead, result.output);
    if (!result.timedOut && result.started) {
        DWORD exitCode = 0;
        if (GetExitCodeProcess(processInfo.hProcess, &exitCode)) {
            result.exitCode = static_cast<int>(exitCode);
        } else {
            result.started = false;
            result.exitCode = static_cast<int>(GetLastError());
        }
    } else if (result.timedOut) {
        result.exitCode = ERROR_TIMEOUT;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(outputRead);
    return result;
}

}  // namespace PocketPad::Adb

#endif  // _WIN32
