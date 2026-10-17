#include "mikrotik_device.h"

#include <string>
#include <utility>

namespace ohboyscan {

MikrotikDevice::MikrotikDevice(std::string ip, std::uint16_t port)
    : m_ip(std::move(ip)), m_port(port) {}

void MikrotikDevice::set_ip(std::string ip) { m_ip = std::move(ip); }

void MikrotikDevice::set_port(std::uint16_t port) noexcept { m_port = port; }

void MikrotikDevice::set_credentials(std::string username, std::string password) {
    m_username = std::move(username);
    m_password = std::move(password);
}

void MikrotikDevice::set_identity(std::string identity) {
    m_identity = std::move(identity);
}

void MikrotikDevice::set_model(std::string model) { m_model = std::move(model); }

void MikrotikDevice::set_routeros_version(std::string version) {
    m_routeros_version = std::move(version);
}

void MikrotikDevice::set_state(State state) noexcept { m_state = state; }

bool MikrotikDevice::valid() const noexcept {
    return !m_ip.empty() && !m_username.empty();
}

std::string MikrotikDevice::rest_base_url() const {
    return "https://" + m_ip + ":" + std::to_string(m_port) + "/rest";
}

} // namespace ohboyscan
