#include "esp_udp.h"
#include <esp_log.h>
#include <lwip/sockets.h>
#include <sys/select.h>
#include <cstring>
#include <arpa/inet.h>

static const char* TAG = "EspUdp";

EspUdp::EspUdp()
    : sock_(-1)
    , port_(0)
    , connected_(false)
    , running_(false)
    , receive_task_(nullptr)
    , task_exit_sem_(xSemaphoreCreateBinary()) {
}

EspUdp::~EspUdp() {
    running_ = false;
    if (sock_ >= 0) {
        close(sock_);
        sock_ = -1;
    }
    if (receive_task_) {
        xSemaphoreTake(task_exit_sem_, pdMS_TO_TICKS(2000));
    }
    if (task_exit_sem_) {
        vSemaphoreDelete(task_exit_sem_);
    }
}

void EspUdp::Connect(const std::string& server, int port) {
    server_ = server;
    port_ = port;

    sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock_ < 0) {
        ESP_LOGE(TAG, "Failed to create UDP socket");
        return;
    }

    struct sockaddr_in dest_addr;
    dest_addr.sin_addr.s_addr = inet_addr(server.c_str());
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(port);

    if (connect(sock_, (struct sockaddr*)&dest_addr, sizeof(dest_addr)) < 0) {
        ESP_LOGE(TAG, "Failed to connect UDP socket");
        close(sock_);
        sock_ = -1;
        return;
    }

    connected_ = true;
    running_ = true;
    ESP_LOGI(TAG, "UDP connected to %s:%d", server.c_str(), port);

    xTaskCreate(ReceiveTask, "udp_recv", 4096, this, 5, &receive_task_);
}

int EspUdp::Send(const std::string& data) {
    if (sock_ < 0) return -1;
    int ret = send(sock_, data.c_str(), data.size(), 0);
    if (ret < 0) {
        ESP_LOGE(TAG, "UDP send failed: %d", errno);
    }
    return ret;
}

void EspUdp::OnMessage(std::function<void(const std::string& data)> callback) {
    on_message_ = std::move(callback);
}

void EspUdp::ReceiveTask(void* arg) {
    auto* self = static_cast<EspUdp*>(arg);
    char buffer[2048];

    while (self->running_ && self->sock_ >= 0) {
        struct timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(self->sock_, &readfds);

        int ret = select(self->sock_ + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0) {
            if (errno == EINTR) continue;
            break;
        }
        if (ret == 0) continue;

        int len = recv(self->sock_, buffer, sizeof(buffer), 0);
        if (len <= 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (self->on_message_) {
            self->on_message_(std::string(buffer, len));
        }
    }

    ESP_LOGI(TAG, "UDP receive task stopped");
    self->receive_task_ = nullptr;
    xSemaphoreGive(self->task_exit_sem_);
    vTaskDelete(NULL);
}
