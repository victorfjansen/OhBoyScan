#pragma once

#include <chrono>
#include <string>
#include <vector>

namespace ohboyscan {

// Feature 2: passively listens for layer-2 discovery packets (MNDP and LLDP)
// and reports the network neighbors it hears on the wire.
class TopologyScanner {
public:
    struct DiscoveredDevice {
        std::string identity;
        std::string model;
        std::string version;
        std::string ip;
        std::string interface;
        std::string protocol; // "MNDP" or "LLDP"
    };

    struct ScanResult {
        std::vector<DiscoveredDevice> devices;
        std::string error; // empty on success
    };

    // Captures on the given interface for `duration` and returns the unique
    // devices discovered. An empty interface captures on all interfaces.
    ScanResult scan(const std::string& interface,
                    std::chrono::milliseconds duration) const;
};

} // namespace ohboyscan
