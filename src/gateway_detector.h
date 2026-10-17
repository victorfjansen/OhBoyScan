#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace ohboyscan {

// Auto-detects the machine's default IPv4 gateway by reading
// /proc/net/route (available on desktop Linux and Raspberry Pi Linux).
class GatewayDetector {
public:
    struct Gateway {
        std::string ip;
        std::string interface;
    };

    std::optional<Gateway> detect_default_gateway() const;

    // Returns the first IPv4 address assigned to the named interface.
    std::optional<std::string> local_ipv4(const std::string& interface) const;

private:
    static std::string ipv4_to_string(std::uint32_t addr);
};

} // namespace ohboyscan
