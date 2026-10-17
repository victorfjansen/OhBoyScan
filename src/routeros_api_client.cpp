#include "routeros_api_client.h"

#include <curl/curl.h>

#include <cstddef>
#include <string>
#include <utility>

namespace ohboyscan {
namespace {

// RAII wrapper that keeps libcurl's global state alive exactly once per
// process and cleans it up at exit.
class CurlGlobal {
public:
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }
};

CurlGlobal& curl_global() {
    static CurlGlobal instance;
    return instance;
}

std::size_t write_callback(char* ptr, std::size_t size, std::size_t nmemb,
                           void* userdata) {
    const auto total = size * nmemb;
    auto* body = static_cast<std::string*>(userdata);
    body->append(ptr, total);
    return total;
}

std::string build_url(const MikrotikDevice& device, const std::string& path) {
    std::string url = device.rest_base_url();
    if (!path.empty() && path.front() != '/') {
        url += '/';
    }
    url += path;
    return url;
}

} // namespace

RouterOsApiClient::RouterOsApiClient(MikrotikDevice device)
    : m_device(std::move(device)) {
    curl_global();
    m_curl = curl_easy_init();
}

RouterOsApiClient::~RouterOsApiClient() { cleanup(); }

RouterOsApiClient::RouterOsApiClient(RouterOsApiClient&& other) noexcept
    : m_device(std::move(other.m_device)), m_curl(other.m_curl) {
    other.m_curl = nullptr;
}

RouterOsApiClient& RouterOsApiClient::operator=(RouterOsApiClient&& other) noexcept {
    if (this != &other) {
        cleanup();
        m_device = std::move(other.m_device);
        m_curl = other.m_curl;
        other.m_curl = nullptr;
    }
    return *this;
}

void RouterOsApiClient::cleanup() noexcept {
    if (m_curl != nullptr) {
        curl_easy_cleanup(static_cast<CURL*>(m_curl));
        m_curl = nullptr;
    }
}

RouterOsApiClient::Response RouterOsApiClient::get(const std::string& path) {
    return perform("GET", build_url(m_device, path), {});
}

RouterOsApiClient::Response RouterOsApiClient::post(const std::string& path,
                                                    const std::string& json_body) {
    return perform("POST", build_url(m_device, path), json_body);
}

RouterOsApiClient::Response RouterOsApiClient::perform(const std::string& method,
                                                       const std::string& url,
                                                       const std::string& body) {
    Response response;
    if (m_curl == nullptr) {
        return response;
    }

    auto* curl = static_cast<CURL*>(m_curl);
    curl_easy_reset(curl);

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
    curl_easy_setopt(curl, CURLOPT_USERNAME, m_device.username().c_str());
    curl_easy_setopt(curl, CURLOPT_PASSWORD, m_device.password().c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPAUTH, CURLAUTH_BASIC);

    // Field routers present self-signed certificates; accept them for now.
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    curl_slist* headers = nullptr;
    if (method == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    const auto code = curl_easy_perform(curl);
    if (code == CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.status_code);
    }

    curl_slist_free_all(headers);
    return response;
}

} // namespace ohboyscan
