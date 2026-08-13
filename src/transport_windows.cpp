#include "transport.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <cstdlib>
#include <stdexcept>
#include <string>

namespace krypticdev {
namespace {

bool is_pipe_path(const std::string& path) {
    return path.rfind("\\\\.\\pipe\\", 0) == 0;
}

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

std::string last_error_message(const std::string& prefix) {
    const DWORD code = GetLastError();
    char* buffer = nullptr;
    const DWORD size = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        code,
        0,
        reinterpret_cast<LPSTR>(&buffer),
        0,
        nullptr);
    std::string detail = size && buffer ? std::string(buffer, size) : std::to_string(code);
    if (buffer) LocalFree(buffer);
    while (!detail.empty() && (detail.back() == '\n' || detail.back() == '\r' || detail.back() == ' ')) {
        detail.pop_back();
    }
    return prefix + ": " + detail;
}

}  // namespace

std::string socket_path() {
    if (const char* override_path = std::getenv("KRYPTIC_SOCKET_PATH")) {
        if (override_path[0] != '\0') return override_path;
    }
    return "\\\\.\\pipe\\kryptic-daemon";
}

std::string round_trip(const std::string& line, std::chrono::milliseconds timeout) {
    const std::string path = socket_path();
    if (!is_pipe_path(path)) {
        fail("KRYPTIC_SOCKET_PATH on Windows must be a named pipe (\\\\.\\pipe\\...)");
    }

    const ULONGLONG deadline = GetTickCount64() + static_cast<ULONGLONG>(timeout.count());
    HANDLE pipe = INVALID_HANDLE_VALUE;
    while (pipe == INVALID_HANDLE_VALUE) {
        pipe = CreateFileA(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_EXISTING,
            0,
            nullptr);
        if (pipe != INVALID_HANDLE_VALUE) break;
        if (GetTickCount64() >= deadline) {
            fail("timed out connecting to the daemon pipe");
        }
        Sleep(50);
    }

    const std::string payload = line.back() == '\n' ? line : line + '\n';
    DWORD written = 0;
    if (!WriteFile(pipe, payload.data(), static_cast<DWORD>(payload.size()), &written, nullptr)) {
        CloseHandle(pipe);
        fail(last_error_message("WriteFile"));
    }

    std::string received;
    char buffer[4096];
    while (received.find('\n') == std::string::npos) {
        DWORD read = 0;
        if (!ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr)) {
            CloseHandle(pipe);
            fail(last_error_message("ReadFile"));
        }
        if (read == 0) {
            CloseHandle(pipe);
            fail("connection closed");
        }
        received.append(buffer, read);
    }

    CloseHandle(pipe);
    return received.substr(0, received.find('\n'));
}

}  // namespace krypticdev
