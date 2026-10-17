#pragma once

#include <cstdint>
#include <string>

namespace ohboyscan {

// Models a single target MikroTik router (RouterOS v7).
// Pure data/state holder with encapsulated accessors; all members are RAII.
class MikrotikDevice {
public:
    enum class State {
        Unknown,
        Reachable,
        Unreachable,
        Authenticated,
    };

    MikrotikDevice() = default;
    explicit MikrotikDevice(std::string ip, std::uint16_t port = 443);

    const std::string& ip() const noexcept { return m_ip; }
    std::uint16_t port() const noexcept { return m_port; }
    const std::string& username() const noexcept { return m_username; }
    const std::string& password() const noexcept { return m_password; }
    const std::string& identity() const noexcept { return m_identity; }
    const std::string& model() const noexcept { return m_model; }
    const std::string& routeros_version() const noexcept { return m_routeros_version; }
    State state() const noexcept { return m_state; }

    void set_ip(std::string ip);
    void set_port(std::uint16_t port) noexcept;
    void set_credentials(std::string username, std::string password);
    void set_identity(std::string identity);
    void set_model(std::string model);
    void set_routeros_version(std::string version);
    void set_state(State state) noexcept;

    // A device is scannable when it has an address and a username.
    bool valid() const noexcept;

    // REST base URL, e.g. "https://192.168.88.1:443/rest".
    std::string rest_base_url() const;

private:
    std::string m_ip;
    std::uint16_t m_port = 443;
    std::string m_username = "admin";
    std::string m_password;
    std::string m_identity;
    std::string m_model;
    std::string m_routeros_version;
    State m_state = State::Unknown;
};

} // namespace ohboyscan
