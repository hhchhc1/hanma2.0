#include "http_client.h"
#include <esp_log.h>
#include <esp_crt_bundle.h>

static const char* TAG = "HttpClient";

static esp_err_t http_event_handler(esp_http_client_event_t* evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ON_CONNECTED:
        case HTTP_EVENT_HEADERS_SENT:
        case HTTP_EVENT_ON_FINISH:
            break;
        default:
            break;
    }
    return ESP_OK;
}

HttpClient::HttpClient()
    : client_(nullptr)
    , status_code_(0)
    , content_length_(0)
    , opened_(false) {
}

HttpClient::~HttpClient() {
    Close();
}

void HttpClient::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpClient::SetContent(std::string&& data) {
    content_ = std::move(data);
}

bool HttpClient::Open(const std::string& method, const std::string& url) {
    if (opened_) {
        Close();
    }

    esp_http_client_config_t config = {};
    config.url = url.c_str();
    config.event_handler = http_event_handler;
    config.method = (method == "POST") ? HTTP_METHOD_POST : HTTP_METHOD_GET;
    config.buffer_size = 4096;
    config.buffer_size_tx = 2048;
    config.crt_bundle_attach = esp_crt_bundle_attach;

    client_ = esp_http_client_init(&config);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return false;
    }

    for (auto& [key, value] : headers_) {
        esp_http_client_set_header(client_, key.c_str(), value.c_str());
    }

    esp_err_t err = esp_http_client_open(client_, content_.size());
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client_);
        client_ = nullptr;
        return false;
    }

    if (!content_.empty()) {
        int written = esp_http_client_write(client_, content_.data(), content_.size());
        if (written != (int)content_.size()) {
            ESP_LOGE(TAG, "Failed to write HTTP body: %d/%d", written, (int)content_.size());
            esp_http_client_cleanup(client_);
            client_ = nullptr;
            return false;
        }
    }

    content_length_ = esp_http_client_fetch_headers(client_);
    status_code_ = esp_http_client_get_status_code(client_);
    opened_ = true;
    return true;
}

int HttpClient::GetStatusCode() const {
    return status_code_;
}

size_t HttpClient::GetBodyLength() const {
    if (!client_) return 0;
    return esp_http_client_get_content_length(client_);
}

int HttpClient::Read(char* buffer, size_t size) {
    if (!client_) return -1;
    int read_len = esp_http_client_read(client_, buffer, size);
    return read_len;
}

int HttpClient::Write(const char* data, size_t size) {
    if (!client_) return -1;
    return esp_http_client_write(client_, data, size);
}

std::string HttpClient::ReadAll() {
    std::string result;
    char buffer[512];
    while (true) {
        int ret = Read(buffer, sizeof(buffer));
        if (ret <= 0) break;
        result.append(buffer, ret);
    }
    return result;
}

void HttpClient::Close() {
    if (client_) {
        esp_http_client_cleanup(client_);
        client_ = nullptr;
    }
    opened_ = false;
}
