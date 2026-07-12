#pragma once

#include "web_socket.h"
#include <string>
#include <functional>
#include <map>
#include "esp_websocket_client.h"

class EspWebSocket : public WebSocket {
    friend void ws_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data);
public:
    EspWebSocket();
    ~EspWebSocket();

    bool IsConnected() const override;
    bool Send(const char* data, size_t len, bool binary) override;
    bool Send(const std::string& text) override;
    void SetHeader(const std::string& key, const std::string& value) override;
    void OnData(std::function<void(const char* data, size_t len, bool binary)> callback) override;
    void OnDisconnected(std::function<void()> callback) override;
    bool Connect(const std::string& url) override;

private:
    esp_websocket_client_handle_t client_;
    std::map<std::string, std::string> headers_;
    std::function<void(const char*, size_t, bool)> on_data_;
    std::function<void()> on_disconnected_;
    bool connected_;
};
