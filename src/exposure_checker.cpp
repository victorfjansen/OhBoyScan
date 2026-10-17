#include "exposure_checker.h"

#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace ohboyscan {
namespace {

// RAII wrapper around a POSIX socket descriptor.
class Socket {
public:
    Socket() : m_fd(::socket(AF_INET, SOCK_STREAM, 0)) {}
    ~Socket() {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }

    Socket(const Socket&) = delete;
    Socket& operator=(const Socket&) = delete;

    int get() const noexcept { return m_fd; }

private:
    int m_fd;
};

} // namespace

std::vector<ExposureChecker::PortFinding> ExposureChecker::check(
    const MikrotikDevice& target) const {
    struct PortSpec {
        const char* name;
        int port;
    };
    static constexpr PortSpec kPorts[] = {
        {"WebFig HTTP", 80},
        {"WebFig SSL", 443},
        {"Winbox", 8291},
        {"API", 8728},
        {"SSH", 22},
        {"Telnet", 23},
    };

    std::vector<PortFinding> findings;
    findings.reserve(6);
    for (const auto& spec : kPorts) {
        findings.push_back(
            {spec.name, spec.port, is_port_open(target.ip(), spec.port, 700)});
    }
    return findings;
}

bool ExposureChecker::is_port_open(const std::string& ip, int port,
                                   int timeout_ms) {
    Socket socket;
    if (socket.get() < 0) {
        return false;
    }

    const int flags = ::fcntl(socket.get(), F_GETFL, 0);
    ::fcntl(socket.get(), F_SETFL, flags | O_NONBLOCK);

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<std::uint16_t>(port));
    if (::inet_pton(AF_INET, ip.c_str(), &addr.sin_addr) != 1) {
        return false;
    }

    const int result =
        ::connect(socket.get(), reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result == 0) {
        return true; // connected immediately
    }
    if (errno != EINPROGRESS) {
        return false;
    }

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(socket.get(), &write_fds);

    timeval timeout{};
    timeout.tv_sec = timeout_ms / 1000;
    timeout.tv_usec = (timeout_ms % 1000) * 1000;

    const int ready =
        ::select(socket.get() + 1, nullptr, &write_fds, nullptr, &timeout);
    if (ready <= 0) {
        return false;
    }

    int so_error = 0;
    socklen_t length = sizeof(so_error);
    ::getsockopt(socket.get(), SOL_SOCKET, SO_ERROR, &so_error, &length);
    return so_error == 0;
}

} // namespace ohboyscan
