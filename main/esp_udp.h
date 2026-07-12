#pragma once

#include "udp.h"
#include <string>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

class EspUdp : public Udp {
public:
    EspUdp();
    ~EspUdp();

    int Send(const std::string& data) override;
    void OnMessage(std::function<void(const std::string& data)> callback) override;
    void Connect(const std::string& server, int port) override;

private:
    static void ReceiveTask(void* arg);

    int sock_;
    std::string server_;
    int port_;
    bool connected_;
    volatile bool running_;
    TaskHandle_t receive_task_;
    SemaphoreHandle_t task_exit_sem_;
    std::function<void(const std::string& data)> on_message_;
};
