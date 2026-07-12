#pragma once

#include <string>
#include <functional>

class Mqtt {
public:
    virtual ~Mqtt() = default;
    virtual void SetKeepAlive(int interval) = 0;
    virtual void OnDisconnected(std::function<void()> callback) = 0;
    virtual void OnConnected(std::function<void()> callback) = 0;
    virtual void OnMessage(std::function<void(const std::string& topic, const std::string& payload)> callback) = 0;
    virtual bool Connect(const std::string& host, int port,
                         const std::string& client_id, const std::string& username,
                         const std::string& password) = 0;
    virtual bool Publish(const std::string& topic, const std::string& payload) = 0;
    virtual bool IsConnected() const = 0;
};
