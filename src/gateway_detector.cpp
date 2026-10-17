#include "gateway_detector.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <netinet/in.h>

#include <fstream>
#include <sstream>

namespace ohboyscan {
namespace {

// RAII wrapper around getifaddrs().
class IfAddrs {
public:
    IfAddrs() { ::getifaddrs(&m_list); }
    ~IfAddrs() {
        if (m_list != nullptr) {
            ::freeifaddrs(m_list);
        }
    }

    IfAddrs(const IfAddrs&) = delete;
    IfAddrs& operator=(const IfAddrs&) = delete;

    ifaddrs* get() const noexcept { return m_list; }
    bool ok() const noexcept { return m_list != nullptr; }

private:
    ifaddrs* m_list = nullptr;
};

} // namespace

std::string GatewayDetector::ipv4_to_string(std::uint32_t addr) {
    in_addr in{};
    in.s_addr = addr;
    char buffer[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, &in, buffer, sizeof(buffer)) == nullptr) {
        return {};
    }
    return buffer;
}

std::optional<GatewayDetector::Gateway> GatewayDetector::detect_default_gateway() const {
    std::ifstream route("/proc/net/route");
    if (!route.is_open()) {
        return std::nullopt;
    }

    std::string line;
    std::getline(route, line); // skip the header row

    constexpr std::uint32_t kRouteUp = 0x0001;

    while (std::getline(route, line)) {
        std::istringstream stream(line);
        std::string iface;
        std::uint32_t destination = 0;
        std::uint32_t gateway = 0;
        std::uint32_t flags = 0;

        if (!(stream >> iface >> std::hex >> destination >> gateway >> flags)) {
            continue;
        }

        // Default route: destination 0.0.0.0, a non-zero gateway, and UP.
        if (destination == 0 && gateway != 0 && (flags & kRouteUp) != 0) {
            return Gateway{ipv4_to_string(gateway), iface};
        }
    }

    return std::nullopt;
}

std::optional<std::string> GatewayDetector::local_ipv4(
    const std::string& interface) const {
    IfAddrs ifaddrs;
    if (!ifaddrs.ok()) {
        return std::nullopt;
    }

    for (auto* entry = ifaddrs.get(); entry != nullptr; entry = entry->ifa_next) {
        if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET) {
            continue;
        }
        if (entry->ifa_name != interface) {
            continue;
        }

        const auto* addr = reinterpret_cast<const sockaddr_in*>(entry->ifa_addr);
        char buffer[INET_ADDRSTRLEN] = {};
        if (::inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer)) == nullptr) {
            continue;
        }
        return std::string(buffer);
    }

    return std::nullopt;
}

} // namespace ohboyscan
