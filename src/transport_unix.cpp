#include "transport.hpp"

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/un.h>
#include <unistd.h>

namespace krypticdev {
namespace {

struct Fd {
    int fd = -1;
    explicit Fd(int value) : fd(value) {}
    ~Fd() {
        if (fd >= 0) ::close(fd);
    }
    Fd(const Fd&) = delete;
    Fd& operator=(const Fd&) = delete;
};

[[noreturn]] void fail(const std::string& message) {
    throw std::runtime_error(message);
}

std::string errno_message(const std::string& prefix) {
    return prefix + ": " + std::strerror(errno);
}

}  // namespace

std::string socket_path() {
    if (const char* override_path = std::getenv("KRYPTIC_SOCKET_PATH")) {
        if (override_path[0] != '\0') return override_path;
    }
#if defined(__linux__)
    if (const char* runtime_dir = std::getenv("XDG_RUNTIME_DIR")) {
        if (runtime_dir[0] != '\0') {
            return std::string(runtime_dir) + "/kryptic-daemon.sock";
        }
    }
#endif
    return "/tmp/kryptic-daemon.sock";
}

std::string round_trip(const std::string& line, std::chrono::milliseconds timeout) {
    const std::string path = socket_path();

    Fd sock(::socket(AF_UNIX, SOCK_STREAM, 0));
    if (sock.fd < 0) fail(errno_message("socket"));

    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    if (path.size() >= sizeof(addr.sun_path)) {
        fail("socket path is too long");
    }
    std::memcpy(addr.sun_path, path.c_str(), path.size() + 1);

    const int flags = ::fcntl(sock.fd, F_GETFL, 0);
    if (flags < 0) fail(errno_message("fcntl"));
    if (::fcntl(sock.fd, F_SETFL, flags | O_NONBLOCK) < 0) fail(errno_message("fcntl"));

    int rc = ::connect(sock.fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (rc < 0 && errno != EINPROGRESS) {
        fail(errno_message("connect"));
    }

    pollfd pfd{};
    pfd.fd = sock.fd;
    pfd.events = POLLOUT;
    rc = ::poll(&pfd, 1, static_cast<int>(timeout.count()));
    if (rc == 0) fail("timed out connecting to the daemon socket");
    if (rc < 0) fail(errno_message("poll"));

    int err = 0;
    socklen_t err_len = sizeof(err);
    if (::getsockopt(sock.fd, SOL_SOCKET, SO_ERROR, &err, &err_len) < 0) {
        fail(errno_message("getsockopt"));
    }
    if (err != 0) {
        errno = err;
        fail(errno_message("connect"));
    }

    if (::fcntl(sock.fd, F_SETFL, flags) < 0) fail(errno_message("fcntl"));

    timeval tv{};
    tv.tv_sec = static_cast<time_t>(timeout.count() / 1000);
    tv.tv_usec = static_cast<suseconds_t>((timeout.count() % 1000) * 1000);
    ::setsockopt(sock.fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    ::setsockopt(sock.fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    const std::string payload = line.back() == '\n' ? line : line + '\n';
    std::size_t written = 0;
    while (written < payload.size()) {
        const ssize_t n = ::send(sock.fd, payload.data() + written, payload.size() - written, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            fail(errno_message("send"));
        }
        written += static_cast<std::size_t>(n);
    }

    std::string received;
    char buffer[4096];
    while (received.find('\n') == std::string::npos) {
        const ssize_t n = ::recv(sock.fd, buffer, sizeof(buffer), 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            if (errno == EAGAIN || errno == EWOULDBLOCK) fail("timed out reading from the daemon");
            fail(errno_message("recv"));
        }
        if (n == 0) fail("connection closed");
        received.append(buffer, static_cast<std::size_t>(n));
    }
    return received.substr(0, received.find('\n'));
}

}  // namespace krypticdev
