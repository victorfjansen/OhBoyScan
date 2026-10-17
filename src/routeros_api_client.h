#pragma once

#include <string>

#include "mikrotik_device.h"

namespace ohboyscan {

// Talks to the RouterOS v7 REST API over HTTPS using libcurl.
// The libcurl handle is hidden behind a void* so this header stays
// dependency-free; construction, cleanup and move semantics are RAII-safe.
class RouterOsApiClient {
public:
    struct Response {
        long status_code = 0;
        std::string body;

        bool ok() const noexcept { return status_code >= 200 && status_code < 300; }
    };

    explicit RouterOsApiClient(MikrotikDevice device);
    ~RouterOsApiClient();

    RouterOsApiClient(const RouterOsApiClient&) = delete;
    RouterOsApiClient& operator=(const RouterOsApiClient&) = delete;

    RouterOsApiClient(RouterOsApiClient&& other) noexcept;
    RouterOsApiClient& operator=(RouterOsApiClient&& other) noexcept;

    Response get(const std::string& path);
    Response post(const std::string& path, const std::string& json_body);

    const MikrotikDevice& device() const noexcept { return m_device; }
    bool valid() const noexcept { return m_curl != nullptr; }

private:
    Response perform(const std::string& method, const std::string& url,
                     const std::string& body);
    void cleanup() noexcept;

    MikrotikDevice m_device;
    void* m_curl = nullptr; // CURL*
};

} // namespace ohboyscan
