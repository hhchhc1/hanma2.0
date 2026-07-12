#include "esp_mqtt.h"
#include <esp_crt_bundle.h>
#include <esp_log.h>
#include <cstring>

static const char* TAG = "EspMqtt";

EspMqtt::EspMqtt()
    : keep_alive_(120)
    , connected_(false)
    , client_handle_(nullptr) {
}

EspMqtt::~EspMqtt() {
    if (client_handle_) {
        esp_mqtt_client_stop(client_handle_);
        esp_mqtt_client_destroy(client_handle_);
    }
}

void EspMqtt::SetKeepAlive(int interval) {
    keep_alive_ = interval;
}

void EspMqtt::OnDisconnected(std::function<void()> callback) {
    on_disconnected_ = std::move(callback);
}

void EspMqtt::OnConnected(std::function<void()> callback) {
    on_connected_ = std::move(callback);
}

void EspMqtt::OnMessage(std::function<void(const std::string& topic, const std::string& payload)> callback) {
    on_message_ = std::move(callback);
}

void EspMqtt::EventCallback(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
    auto* self = static_cast<EspMqtt*>(handler_args);
    auto* event = static_cast<esp_mqtt_event_t*>(event_data);

    switch (event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            self->connected_ = true;
            if (self->on_connected_) self->on_connected_();
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");
            self->connected_ = false;
            if (self->on_disconnected_) self->on_disconnected_();
            break;
        case MQTT_EVENT_DATA:
            if (self->on_message_) {
                self->on_message_(std::string(event->topic, event->topic_len),
                                  std::string(event->data, event->data_len));
            }
            break;
        default:
            break;
    }
}

bool EspMqtt::Connect(const std::string& host, int port,
                       const std::string& client_id, const std::string& username,
                       const std::string& password) {
    if (client_handle_) {
        esp_mqtt_client_stop(client_handle_);
        esp_mqtt_client_destroy(client_handle_);
        client_handle_ = nullptr;
    }

    esp_mqtt_client_config_t config = {};
    config.broker.address.hostname = host.c_str();
    config.broker.address.port = port;
    config.broker.address.transport = (port == 8883) ? MQTT_TRANSPORT_OVER_SSL : MQTT_TRANSPORT_OVER_TCP;
    config.credentials.client_id = client_id.c_str();
    config.credentials.username = username.c_str();
    config.credentials.authentication.password = password.c_str();
    config.session.keepalive = keep_alive_;

    if (port == 8883) {
        config.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
    }

    client_handle_ = esp_mqtt_client_init(&config);
    if (!client_handle_) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return false;
    }

    esp_mqtt_client_register_event(client_handle_, MQTT_EVENT_ANY, EventCallback, this);

    esp_err_t err = esp_mqtt_client_start(client_handle_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start MQTT client: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(client_handle_);
        client_handle_ = nullptr;
        return false;
    }

    return true;
}

bool EspMqtt::Publish(const std::string& topic, const std::string& payload) {
    if (!client_handle_ || !connected_) return false;
    int msg_id = esp_mqtt_client_publish(client_handle_, topic.c_str(), payload.data(), payload.size(), 0, 0);
    return msg_id >= 0;
}

bool EspMqtt::IsConnected() const {
    return connected_;
}
