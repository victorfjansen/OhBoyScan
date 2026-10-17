#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "mikrotik_device.h"

namespace ohboyscan {

// Feature 5: checks which MikroTik management ports are exposed on the
// target and flags the insecure ones.
class ExposureChecker {
public:
    struct PortFinding {
        std::string name;
        int port;
        bool open;
    };

    std::vector<PortFinding> check(const MikrotikDevice& target) const;

private:
    static bool is_port_open(const std::string& ip, int port, int timeout_ms);
};

} // namespace ohboyscan
