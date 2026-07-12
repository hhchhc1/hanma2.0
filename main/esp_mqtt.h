#pragma once

#include "mqtt.h"
#include <mqtt_client.h>
#include <string>
#include <functional>

class EspMqtt : public Mqtt {
public:
    EspMqtt();
    ~EspMqtt();

    void SetKeepAlive(int interval) override;
    void OnDisconnected(std::function<void()> callback) override;
    void OnConnected(std::function<void()> callback) override;
    void OnMessage(std::function<void(const std::string& topic, const std::string& payload)> callback) override;
    bool Connect(const std::string& host, int port,
                 const std::string& client_id, const std::string& username,
                 const std::string& password) override;
    bool Publish(const std::string& topic, const std::string& payload) override;
    bool IsConnected() const override;

private:
    static void EventCallback(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);

    int keep_alive_;
    bool connected_;
    esp_mqtt_client_handle_t client_handle_;
    std::function<void()> on_connected_;
    std::function<void()> on_disconnected_;
    std::function<void(const std::string& topic, const std::string& payload)> on_message_;
};
