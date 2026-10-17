#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

namespace ohboyscan {

// Feature 3: continuously watches ARP traffic for gateway MAC forgery
// (ARP spoofing/poisoning). Runs its own capture thread; RAII-stopped.
class ArpMonitor {
public:
    struct Event {
        std::string message;
        bool alert = false; // true = possible spoofing, false = info
    };

    using EventCallback = std::function<void(const Event&)>;

    ArpMonitor() = default;
    ~ArpMonitor();

    ArpMonitor(const ArpMonitor&) = delete;
    ArpMonitor& operator=(const ArpMonitor&) = delete;

    void set_event_callback(EventCallback callback);

    // Starts capturing on `interface`, watching `gateway_ip`. Returns false
    // if the capture socket could not be opened (e.g. missing privileges).
    bool start(const std::string& interface, const std::string& gateway_ip);

    void stop();
    bool running() const noexcept { return m_thread.joinable(); }

private:
    void capture_loop();
    void emit_event(Event event);

    EventCallback m_callback;
    std::mutex m_callback_mutex;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    std::string m_interface;
    std::string m_gateway_ip;
    int m_socket = -1;
};

} // namespace ohboyscan
