#pragma once

#include <string>
#include <functional>

class Udp {
public:
    virtual ~Udp() = default;
    virtual int Send(const std::string& data) = 0;
    virtual void OnMessage(std::function<void(const std::string& data)> callback) = 0;
    virtual void Connect(const std::string& server, int port) = 0;
};
