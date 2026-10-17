#include "arp_monitor.h"

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
#include <cstdint>
#include <optional>
#include <utility>

namespace ohboyscan {
namespace {

constexpr std::uint16_t kEtherTypeVlan = 0x8100;
constexpr std::uint16_t kEtherTypeQinQ = 0x88A8;
constexpr std::uint16_t kEtherTypeArp = 0x0806;

std::uint16_t read_be16(const std::uint8_t* data) {
    return static_cast<std::uint16_t>((data[0] << 8) | data[1]);
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

// Parses the sender fields of an ARP packet.
class ArpPacketParser {
public:
    struct Packet {
        std::string sender_mac;
        std::string sender_ip;
    };

    static std::optional<Packet> parse(const std::uint8_t* payload,
                                       std::size_t length) {
        if (length < 28) {
            return std::nullopt;
        }
        Packet packet;
        packet.sender_mac = mac_to_string(payload + 8);
        packet.sender_ip = ipv4_to_string(payload + 14);
        return packet;
    }
};

} // namespace

ArpMonitor::~ArpMonitor() { stop(); }

void ArpMonitor::set_event_callback(EventCallback callback) {
    std::lock_guard<std::mutex> lock(m_callback_mutex);
    m_callback = std::move(callback);
}

bool ArpMonitor::start(const std::string& interface, const std::string& gateway_ip) {
    if (m_thread.joinable()) {
        return false;
    }

    const int fd = ::socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    if (fd < 0) {
        return false;
    }

    if (!interface.empty()) {
        const auto index = ::if_nametoindex(interface.c_str());
        if (index == 0) {
            ::close(fd);
            return false;
        }

        sockaddr_ll address{};
        address.sll_family = AF_PACKET;
        address.sll_protocol = htons(ETH_P_ALL);
        address.sll_ifindex = static_cast<int>(index);

        if (::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) < 0) {
            ::close(fd);
            return false;
        }
    }

    m_interface = interface;
    m_gateway_ip = gateway_ip;
    m_socket = fd;
    m_stop = false;

    m_thread = std::thread(&ArpMonitor::capture_loop, this);
    return true;
}

void ArpMonitor::stop() {
    m_stop = true;
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

void ArpMonitor::emit_event(Event event) {
    EventCallback callback;
    {
        std::lock_guard<std::mutex> lock(m_callback_mutex);
        callback = m_callback;
    }
    if (callback) {
        callback(std::move(event));
    }
}

void ArpMonitor::capture_loop() {
    const int fd = m_socket;

    timeval timeout{};
    timeout.tv_sec = 0;
    timeout.tv_usec = 200000; // 200 ms
    ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    std::string baseline_mac;
    std::array<std::uint8_t, 65536> buffer{};

    while (!m_stop) {
        const ssize_t received =
            ::recvfrom(fd, buffer.data(), buffer.size(), 0, nullptr, nullptr);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                continue;
            }
            break;
        }
        if (received < 14 + 28) { // Ethernet header + minimum ARP packet
            continue;
        }

        std::size_t offset = 14;
        auto ethertype = read_be16(buffer.data() + offset);
        while ((ethertype == kEtherTypeVlan || ethertype == kEtherTypeQinQ) &&
               offset + 4 <= static_cast<std::size_t>(received)) {
            offset += 4;
            ethertype = read_be16(buffer.data() + offset);
        }
        if (ethertype != kEtherTypeArp) {
            continue;
        }

        const auto packet = ArpPacketParser::parse(
            buffer.data() + offset, static_cast<std::size_t>(received) - offset);
        if (!packet || packet->sender_ip != m_gateway_ip) {
            continue;
        }

        const auto& mac = packet->sender_mac;
        if (baseline_mac.empty()) {
            baseline_mac = mac;
            emit_event({m_gateway_ip + " MAC: " + mac, false});
        } else if (mac != baseline_mac) {
            emit_event({"ALERT: " + m_gateway_ip + " MAC changed to " + mac, true});
        }
    }

    ::close(fd);
    m_socket = -1;
}

} // namespace ohboyscan
