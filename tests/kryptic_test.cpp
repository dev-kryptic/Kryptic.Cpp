// Tests against a mock daemon: a unix-socket server speaking PROTOCOL.md v1.

#include <kryptic/kryptic.hpp>

#include <atomic>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#ifndef _WIN32
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

namespace {

int g_failed = 0;
int g_passed = 0;

void check(bool condition, const char* expression, const char* file, int line) {
    if (condition) {
        ++g_passed;
        return;
    }
    ++g_failed;
    std::cerr << "FAIL " << file << ":" << line << "  " << expression << '\n';
}

#define CHECK(cond) check(static_cast<bool>(cond), #cond, __FILE__, __LINE__)

void set_env(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    ::setenv(name, value, 1);
#endif
}

void unset_env(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    ::unsetenv(name);
#endif
}

std::filesystem::path make_project_dir() {
    const auto dir = std::filesystem::temp_directory_path() / ("kryptic-sdk-" + std::to_string(::getpid()));
    std::filesystem::create_directories(dir);
    std::ofstream out(dir / "kryptic.json");
    out << "{\"projectId\":\"proj_test123456\"}";
    return dir;
}

#ifndef _WIN32

class MockDaemon {
public:
    explicit MockDaemon(std::function<std::string(const std::string&)> handler)
        : handler_(std::move(handler)) {
        char template_path[] = "/tmp/kdXXXXXX";
        if (!::mkdtemp(template_path)) {
            throw std::runtime_error("mkdtemp failed");
        }
        dir_ = template_path;
        path_ = dir_ + "/d.sock";

        listen_fd_ = ::socket(AF_UNIX, SOCK_STREAM, 0);
        if (listen_fd_ < 0) throw std::runtime_error("socket");

        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        std::strncpy(addr.sun_path, path_.c_str(), sizeof(addr.sun_path) - 1);
        if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
            throw std::runtime_error("bind");
        }
        if (::listen(listen_fd_, 8) < 0) throw std::runtime_error("listen");

        thread_ = std::thread([this] { serve(); });
        set_env("KRYPTIC_SOCKET_PATH", path_.c_str());
    }

    ~MockDaemon() {
        running_ = false;
        if (listen_fd_ >= 0) {
            ::shutdown(listen_fd_, SHUT_RDWR);
            ::close(listen_fd_);
        }
        if (thread_.joinable()) thread_.join();
        if (!path_.empty()) ::unlink(path_.c_str());
        if (!dir_.empty()) ::rmdir(dir_.c_str());
        unset_env("KRYPTIC_SOCKET_PATH");
    }

    const std::string& path() const { return path_; }

private:
    void serve() {
        while (running_) {
            const int client = ::accept(listen_fd_, nullptr, nullptr);
            if (client < 0) return;
            std::string received;
            char buffer[4096];
            while (received.find('\n') == std::string::npos) {
                const ssize_t n = ::recv(client, buffer, sizeof(buffer), 0);
                if (n <= 0) break;
                received.append(buffer, static_cast<std::size_t>(n));
            }
            if (!received.empty()) {
                const std::string request = received.substr(0, received.find('\n'));
                const std::string response = handler_(request) + "\n";
                ::send(client, response.data(), response.size(), 0);
            }
            ::close(client);
        }
    }

    std::function<std::string(const std::string&)> handler_;
    std::string dir_;
    std::string path_;
    int listen_fd_ = -1;
    std::atomic<bool> running_{true};
    std::thread thread_;
};

#endif

class Fixture {
public:
    Fixture() {
        project_dir_ = make_project_dir();
        original_cwd_ = std::filesystem::current_path();
        std::filesystem::current_path(project_dir_);
        set_env("KRYPTIC_SILENT", "true");
        unset_env("KRYPTIC_DISABLED");
        unset_env("KRYPTIC_PROJECT_ID");
        unset_env("KRYPTIC_ENV");
        unset_env("INJECTED_KEY");
        unset_env("EXISTING_KEY");
        unset_env("CPP_ENV");
        unset_env("APP_ENV");
        unset_env("ENVIRONMENT");
        unset_env("ENV");
        unset_env("KRYPTIC_SOCKET_PATH");
    }

    ~Fixture() {
        std::filesystem::current_path(original_cwd_);
        std::error_code ec;
        std::filesystem::remove_all(project_dir_, ec);
        unset_env("INJECTED_KEY");
        unset_env("EXISTING_KEY");
        unset_env("KRYPTIC_SOCKET_PATH");
        unset_env("KRYPTIC_PROJECT_ID");
        unset_env("KRYPTIC_ENV");
        unset_env("KRYPTIC_DISABLED");
        unset_env("CPP_ENV");
    }

    std::filesystem::path project_dir_;
    std::filesystem::path original_cwd_;
};

}  // namespace

int main() {
#ifndef _WIN32
    {
        Fixture fx;
        std::string seen;
        MockDaemon daemon([&](const std::string& request) {
            seen = request;
            return "{\"v\":1,\"ok\":true,\"secrets\":[{\"key\":\"INJECTED_KEY\",\"value\":\"from-daemon\"}]}";
        });
        const auto result = kryptic::inject();
        CHECK(!result.skipped);
        CHECK(result.injected == 1);
        const char* value = std::getenv("INJECTED_KEY");
        CHECK(value && std::string(value) == "from-daemon");
        CHECK(seen.find("\"projectId\":\"proj_test123456\"") != std::string::npos);
        CHECK(seen.find("\"environment\":\"development\"") != std::string::npos);
    }

    {
        Fixture fx;
        set_env("EXISTING_KEY", "real-env-wins");
        MockDaemon daemon([](const std::string&) {
            return "{\"v\":1,\"ok\":true,\"secrets\":[{\"key\":\"EXISTING_KEY\",\"value\":\"x\"}]}";
        });
        const auto result = kryptic::inject();
        CHECK(result.injected == 0);
        const char* value = std::getenv("EXISTING_KEY");
        CHECK(value && std::string(value) == "real-env-wins");
    }

    {
        Fixture fx;
        set_env("KRYPTIC_SOCKET_PATH", (fx.project_dir_ / "missing.sock").string().c_str());
        const auto result = kryptic::inject();
        CHECK(result.skipped);
        CHECK(result.reason == "daemon_unreachable");
    }

    {
        Fixture fx;
        set_env("CPP_ENV", "production");
        const auto result = kryptic::inject();
        CHECK(result.skipped);
        CHECK(result.reason == "cpp_env_production");
    }

    {
        Fixture fx;
        set_env("KRYPTIC_DISABLED", "true");
        const auto result = kryptic::inject();
        CHECK(result.skipped);
        CHECK(result.reason == "disabled");
    }

    {
        Fixture fx;
        MockDaemon daemon([](const std::string&) {
            return "{\"v\":1,\"ok\":false,\"error\":\"access_denied\"}";
        });
        const auto result = kryptic::inject();
        CHECK(result.skipped);
        CHECK(result.reason == "access_denied");
    }

    {
        Fixture fx;
        set_env("KRYPTIC_PROJECT_ID", "proj_override0001");
        set_env("KRYPTIC_ENV", "staging");
        std::string seen;
        MockDaemon daemon([&](const std::string& request) {
            seen = request;
            return "{\"v\":1,\"ok\":true,\"secrets\":[]}";
        });
        kryptic::inject();
        CHECK(seen.find("proj_override0001") != std::string::npos);
        CHECK(seen.find("staging") != std::string::npos);
    }
#else
    {
        Fixture fx;
        set_env("KRYPTIC_DISABLED", "true");
        const auto result = kryptic::inject();
        CHECK(result.skipped);
        CHECK(result.reason == "disabled");
    }
#endif

    if (g_failed > 0) {
        std::cerr << g_failed << " failed, " << g_passed << " passed\n";
        return 1;
    }
    std::cout << g_passed << " passed\n";
    return 0;
}
