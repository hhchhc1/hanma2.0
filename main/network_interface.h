#pragma once

#include <memory>
#include <string>

class Mqtt;
class Udp;
class WebSocket;
class Http;

class NetworkInterface {
public:
    virtual ~NetworkInterface() = default;
    virtual std::unique_ptr<Mqtt> CreateMqtt(int priority) = 0;
    virtual std::unique_ptr<Udp> CreateUdp(int priority) = 0;
    virtual std::unique_ptr<WebSocket> CreateWebSocket(int priority) = 0;
    virtual std::unique_ptr<Http> CreateHttp(int priority) = 0;
};
