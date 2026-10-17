#include "cable_tester.h"

#include "routeros_api_client.h"

#include <string>
#include <utility>
#include <vector>

namespace ohboyscan {
namespace {

// Minimal JSON string-value extractor for the flat objects RouterOS REST
// returns. Finds occurrences of "key": "value".
class JsonExtractor {
public:
    static std::vector<std::string> strings(const std::string& json,
                                            const std::string& key) {
        std::vector<std::string> values;
        const std::string needle = "\"" + key + "\"";
        std::size_t pos = 0;
        while ((pos = json.find(needle, pos)) != std::string::npos) {
            pos += needle.size();

            std::size_t colon = pos;
            while (colon < json.size() &&
                   (json[colon] == ' ' || json[colon] == '\t' ||
                    json[colon] == '\n' || json[colon] == '\r')) {
                ++colon;
            }
            if (colon >= json.size() || json[colon] != ':') {
                continue;
            }

            std::size_t value_start = colon + 1;
            while (value_start < json.size() &&
                   (json[value_start] == ' ' || json[value_start] == '\t')) {
                ++value_start;
            }
            if (value_start >= json.size() || json[value_start] != '"') {
                continue;
            }

            std::size_t value_end = value_start + 1;
            while (value_end < json.size() && json[value_end] != '"') {
                ++value_end;
            }
            if (value_end >= json.size()) {
                continue;
            }

            values.push_back(
                json.substr(value_start + 1, value_end - value_start - 1));
            pos = value_end + 1;
        }
        return values;
    }

    static std::string first_string(const std::string& json,
                                    const std::string& key) {
        const auto values = strings(json, key);
        return values.empty() ? std::string{} : values.front();
    }
};

} // namespace

std::vector<CableTester::CableResult> CableTester::test(
    const MikrotikDevice& target) const {
    std::vector<CableResult> results;

    RouterOsApiClient client(target);
    if (!client.valid()) {
        results.push_back({"REST API", "client error", ""});
        return results;
    }

    const auto list = client.get("interface/ethernet");
    if (!list.ok()) {
        results.push_back(
            {"REST API", "error " + std::to_string(list.status_code), ""});
        return results;
    }

    const auto names = JsonExtractor::strings(list.body, "name");
    if (names.empty()) {
        results.push_back({"REST API", "no interfaces", ""});
        return results;
    }

    for (const auto& name : names) {
        CableResult result;
        result.interface = name;

        const auto response = client.post(
            "interface/ethernet/cable-test",
            "{\"interface\":\"" + name + "\",\"duration\":\"1s\"}");

        if (!response.ok()) {
            result.status = "error " + std::to_string(response.status_code);
        } else {
            result.status = JsonExtractor::first_string(response.body, "status");
            result.distance = JsonExtractor::first_string(response.body, "distance");
            if (result.status.empty()) {
                result.status = "unknown";
            }
        }

        results.push_back(std::move(result));
    }

    return results;
}

} // namespace ohboyscan
