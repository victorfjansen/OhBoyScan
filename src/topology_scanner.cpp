#include "topology_scanner.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <linux/if_ether.h>
#include <linux/if_packet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <array>
#include <chrono>
#include <map>
#include <optional>
#include <utility>

namespace ohboyscan {
namespace {

constexpr std::uint16_t kEtherTypeIpv4 = 0x0800;
constexpr std::uint16_t kEtherTypeVlan = 0x8100;
constexpr std::uint16_t kEtherTypeQinQ = 0x88A8;
constexpr std::uint16_t kEtherTypeLldp = 0x88CC;
constexpr std::uint16_t kMndpPort = 5678;

std::uint16_t read_be16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
}

std::string clean_string(const std::uint8_t* data, std::size_t length) {
    std::string out(reinterpret_cast<const char*>(data), length);
    while (!out.empty() && (out.back() == '\0' || out.back() == ' ' ||
                            out.back() == '\n' || out.back() == '\r')) {
        out.pop_back();
    }
    return out;
}

std::string mac_to_string(const std::uint8_t* mac) {
    char buffer[18] = {};
    std::snprintf(buffer, sizeof(buffer), "%02x:%02x:%02x:%02x:%02x:%02x",
                  mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    return buffer;
}

std::string ipv4_to_string(const std::uint8_t* ip) {
    char buffer[INET_ADDRSTRLEN] = {};
    if (inet_ntop(AF_INET, ip, buffer, sizeof(buffer)) == nullptr) {
        return {};
    }
    return buffer;
}

// RAII wrapper around a raw AF_PACKET socket.
class PacketSocket {
public:
    PacketSocket() : m_fd(::socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) {}
    ~PacketSocket() {
        if (m_fd >= 0) {
            ::close(m_fd);
        }
    }

    PacketSocket(const PacketSocket&) = delete;
    PacketSocket& operator=(const PacketSocket&) = delete;

    int get() const noexcept { return m_fd; }

private:
    int m_fd;
};

// Parses MNDP and LLDP discovery packets into DiscoveredDevice values.
class DiscoveryParser {
public:
    using Device = TopologyScanner::DiscoveredDevice;

    static std::optional<Device> parse_mndp(const std::uint8_t* payload,
                                            std::size_t length);
    static std::optional<Device> parse_lldp(const std::uint8_t* payload,
                                            std::size_t length);
};

std::optional<DiscoveryParser::Device> DiscoveryParser::parse_mndp(
    const std::uint8_t* payload, std::size_t length) {
    Device device;
    device.protocol = "MNDP";

    std::size_t pos = 0;
    bool has_identity = false;
    while (pos + 4 <= length) {
        const auto type = read_be16(payload + pos);
        const auto value_length = read_be16(payload + pos + 2);
        pos += 4;
        if (pos + value_length > length) {
            break;
        }
        const auto* value = payload + pos;

        switch (type) {
        case 5: // identity
            device.identity = clean_string(value, value_length);
            has_identity = !device.identity.empty();
            break;
        case 7: // RouterOS version
            device.version = clean_string(value, value_length);
            break;
        case 8: // platform / RouterBOARD model
            device.model = clean_string(value, value_length);
            break;
        case 10: // local interface name
            device.interface = clean_string(value, value_length);
            break;
        case 11: // IPv4 management address
            if (value_length >= 4) {
                device.ip = ipv4_to_string(value);
            }
            break;
        default:
            break;
        }

        pos += value_length;
    }

    if (!has_identity) {
        return std::nullopt;
    }
    return device;
}

std::optional<DiscoveryParser::Device> DiscoveryParser::parse_lldp(
    const std::uint8_t* payload, std::size_t length) {
    Device device;
    device.protocol = "LLDP";

    std::size_t pos = 0;
    bool has_identity = false;
    while (pos + 2 <= length) {
        const auto header = payload[pos];
        const auto type = (header >> 1) & 0x7F;
        const auto value_length =
            static_cast<std::size_t>(((header & 1) << 8) | payload[pos + 1]);
        pos += 2;

        if (type == 0) { // end of LLDPDU
            break;
        }
        if (pos + value_length > length) {
            break;
        }
        const auto* value = payload + pos;

        switch (type) {
        case 2: // port ID: subtype (1 byte) + value
            if (value_length >= 1) {
                device.interface = clean_string(value + 1, value_length - 1);
            }
            break;
        case 5: // system name
            device.identity = clean_string(value, value_length);
            has_identity = !device.identity.empty();
            break;
        case 6: // system description
            device.model = clean_string(value, value_length);
            break;
        case 8: { // management address
            if (value_length >= 2) {
                const auto address_length = value[0];
                const auto subtype = value[1];
                if (subtype == 1 /* IPv4 */ && address_length >= 5 &&
                    value_length >= static_cast<std::size_t>(address_length) + 1) {
                    device.ip = ipv4_to_string(value + 2);
                }
            }
            break;
        }
        default:
            break;
        }

        pos += value_length;
    }

    if (!has_identity) {
        return std::nullopt;
    }
    return device;
}

} // namespace

TopologyScanner::ScanResult TopologyScanner::scan(
    const std::string& interface, std::chrono::milliseconds duration) const {
    ScanResult result;

    PacketSocket socket;
    if (socket.get() < 0) {
        result.error = "Packet capture requires root or CAP_NET_RAW";
        return result;
    }

    if (!interface.empty()) {
        const auto index = ::if_nametoindex(interface.c_str());
        if (index == 0) {
            result.error = "Unknown interface: " + interface;
            return result;
        }

        sockaddr_ll address{};
        address.sll_family = AF_PACKET;
        address.sll_protocol = htons(ETH_P_ALL);
        address.sll_ifindex = static_cast<int>(index);

        if (::bind(socket.get(), reinterpret_cast<sockaddr*>(&address),
                   sizeof(address)) < 0) {
            result.error = "Failed to bind to interface: " + interface;
            return result;
        }
    }

    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 100000; // 100 ms
    ::setsockopt(socket.get(), SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    std::map<std::string, DiscoveredDevice> by_mac;
    std::array<std::uint8_t, 65536> buffer{};

    const auto deadline = std::chrono::steady_clock::now() + duration;
    while (std::chrono::steady_clock::now() < deadline) {
        const ssize_t received =
            ::recvfrom(socket.get(), buffer.data(), buffer.size(), 0, nullptr, nullptr);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }
        if (received < 14) {
            continue;
        }

        const std::string mac = mac_to_string(buffer.data() + 6);

        std::size_t offset = 14;
        auto ethertype = read_be16(buffer.data() + offset);
        while ((ethertype == kEtherTypeVlan || ethertype == kEtherTypeQinQ) &&
               offset + 4 <= static_cast<std::size_t>(received)) {
            offset += 4;
            ethertype = read_be16(buffer.data() + offset);
        }

        std::optional<DiscoveredDevice> device;
        if (ethertype == kEtherTypeLldp) {
            device = DiscoveryParser::parse_lldp(
                buffer.data() + offset, static_cast<std::size_t>(received) - offset);
        } else if (ethertype == kEtherTypeIpv4) {
            if (static_cast<std::size_t>(received) < offset + 20) {
                continue;
            }
            const auto* ip = buffer.data() + offset;
            const auto ihl = static_cast<std::size_t>(ip[0] & 0x0F) * 4;
            if (ihl < 20 || static_cast<std::size_t>(received) < offset + ihl + 8) {
                continue;
            }
            if (ip[9] != 17 /* UDP */) {
                continue;
            }
            const auto* udp = buffer.data() + offset + ihl;
            const auto source_port = read_be16(udp);
            const auto destination_port = read_be16(udp + 2);
            if (source_port == kMndpPort || destination_port == kMndpPort) {
                const auto payload_offset = offset + ihl + 8;
                device = DiscoveryParser::parse_mndp(
                    buffer.data() + payload_offset,
                    static_cast<std::size_t>(received) - payload_offset);
            }
        }

        if (device && !device->identity.empty()) {
            auto existing = by_mac.find(mac);
            if (existing == by_mac.end()) {
                by_mac.emplace(mac, *device);
            } else if (existing->second.protocol == "LLDP" &&
                       device->protocol == "MNDP") {
                existing->second = *device;
            }
        }
    }

    result.devices.reserve(by_mac.size());
    for (auto& entry : by_mac) {
        result.devices.push_back(std::move(entry.second));
    }
    return result;
}

} // namespace ohboyscan
