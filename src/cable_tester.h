#pragma once

#include <string>
#include <vector>

#include "mikrotik_device.h"

namespace ohboyscan {

// Feature 4: triggers RouterOS `/interface ethernet cable-test` over the REST
// API and reports the per-interface cable status and distance.
class CableTester {
public:
    struct CableResult {
        std::string interface;
        std::string status;   // "link-ok", "open", "short", "unknown", ...
        std::string distance; // e.g. "50m"; empty if unknown
    };

    std::vector<CableResult> test(const MikrotikDevice& target) const;
};

} // namespace ohboyscan
