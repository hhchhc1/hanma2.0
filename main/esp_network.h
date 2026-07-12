#pragma once

#include "network_interface.h"
#include <memory>

class EspNetwork : public NetworkInterface {
public:
    EspNetwork() = default;
    ~EspNetwork() = default;

    std::unique_ptr<Http> CreateHttp(int priority) override;
    std::unique_ptr<Udp> CreateUdp(int priority) override;
    std::unique_ptr<Mqtt> CreateMqtt(int priority) override;
    std::unique_ptr<WebSocket> CreateWebSocket(int priority) override;
};
