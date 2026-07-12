#pragma once

#include <string>
#include <functional>
#include <cstddef>

class WebSocket {
public:
    virtual ~WebSocket() = default;
    virtual bool IsConnected() const = 0;
    virtual bool Send(const char* data, size_t len, bool binary) = 0;
    virtual bool Send(const std::string& text) = 0;
    virtual void SetHeader(const std::string& key, const std::string& value) = 0;
    virtual void OnData(std::function<void(const char* data, size_t len, bool binary)> callback) = 0;
    virtual void OnDisconnected(std::function<void()> callback) = 0;
    virtual bool Connect(const std::string& url) = 0;
};
