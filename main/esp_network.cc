#include "esp_network.h"
#include "http_client.h"
#include "esp_mqtt.h"
#include "esp_udp.h"
#include "esp_websocket.h"

std::unique_ptr<Http> EspNetwork::CreateHttp(int priority) {
    return std::make_unique<HttpClient>();
}

std::unique_ptr<Udp> EspNetwork::CreateUdp(int priority) {
    return std::make_unique<EspUdp>();
}

std::unique_ptr<Mqtt> EspNetwork::CreateMqtt(int priority) {
    return std::make_unique<EspMqtt>();
}

std::unique_ptr<WebSocket> EspNetwork::CreateWebSocket(int priority) {
    return std::make_unique<EspWebSocket>();
}
