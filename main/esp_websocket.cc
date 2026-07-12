#include "esp_websocket.h"
#include <esp_log.h>
#include <cstring>

static const char* TAG = "EspWebSocket";

void ws_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    EspWebSocket* self = (EspWebSocket*)handler_args;
    auto* event = (esp_websocket_event_data_t*)event_data;

    switch (event_id) {
        case WEBSOCKET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "WebSocket connected");
            break;
        case WEBSOCKET_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "WebSocket disconnected");
            if (self->on_disconnected_) self->on_disconnected_();
            break;
        case WEBSOCKET_EVENT_DATA:
            if (self->on_data_) {
                self->on_data_((const char*)event->data_ptr, event->data_len, event->op_code == 2);
            }
            break;
        case WEBSOCKET_EVENT_ERROR:
            ESP_LOGE(TAG, "WebSocket error");
            break;
        default:
            break;
    }
}

EspWebSocket::EspWebSocket()
    : client_(nullptr)
    , connected_(false) {
}

EspWebSocket::~EspWebSocket() {
    if (client_) {
        esp_websocket_client_destroy(client_);
    }
}

void EspWebSocket::SetHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

bool EspWebSocket::Connect(const std::string& url) {
    esp_websocket_client_config_t config = {};
    config.uri = url.c_str();

    std::string header_str;
    if (!headers_.empty()) {
        for (auto& [k, v] : headers_) {
            if (!header_str.empty()) header_str += "\r\n";
            header_str += k + ": " + v;
        }
        config.headers = header_str.c_str();
    }

    client_ = esp_websocket_client_init(&config);
    if (!client_) {
        ESP_LOGE(TAG, "Failed to init WebSocket client");
        return false;
    }

    esp_websocket_register_events(client_, WEBSOCKET_EVENT_ANY, ws_event_handler, this);

    esp_err_t err = esp_websocket_client_start(client_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start WebSocket: %s", esp_err_to_name(err));
        esp_websocket_client_destroy(client_);
        client_ = nullptr;
        return false;
    }

    connected_ = true;
    return true;
}

bool EspWebSocket::IsConnected() const {
    if (client_) {
        return esp_websocket_client_is_connected(client_);
    }
    return false;
}

bool EspWebSocket::Send(const char* data, size_t len, bool binary) {
    if (!client_) return false;
    int ret;
    if (binary) {
        ret = esp_websocket_client_send_bin(client_, data, len, 0);
    } else {
        ret = esp_websocket_client_send_text(client_, data, len, 0);
    }
    return ret >= 0;
}

bool EspWebSocket::Send(const std::string& text) {
    return Send(text.c_str(), text.size(), false);
}

void EspWebSocket::OnData(std::function<void(const char* data, size_t len, bool binary)> callback) {
    on_data_ = std::move(callback);
}

void EspWebSocket::OnDisconnected(std::function<void()> callback) {
    on_disconnected_ = std::move(callback);
}
