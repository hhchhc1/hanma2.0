#include "ec11.h"
#include <esp_log.h>
#include <esp_rom_sys.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "EC11"

static Ec11* g_ec11 = nullptr;

Ec11& Ec11::GetInstance() {
    static Ec11 instance;
    return instance;
}

Ec11::~Ec11() {
    g_ec11 = nullptr;
}

void Ec11::Init(gpio_num_t s1_pin, gpio_num_t s2_pin, gpio_num_t key_pin) {
    s1_pin_ = s1_pin;
    s2_pin_ = s2_pin;
    key_pin_ = key_pin;
    g_ec11 = this;

    gpio_config_t io_conf = {};
    io_conf.pin_bit_mask = (1ULL << s1_pin) | (1ULL << s2_pin) | (1ULL << key_pin);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&io_conf);

    last_s1_ = gpio_get_level(s1_pin_);
    last_key_ = gpio_get_level(key_pin_);

    xTaskCreate(PollingTaskWrapper, "ec11_poll", 2048, this, 5, nullptr);
    ESP_LOGI(TAG, "EC11 initialized: S1=%d, S2=%d, KEY=%d", s1_pin, s2_pin, key_pin);
}

void Ec11::PollingTaskWrapper(void* arg) {
    static_cast<Ec11*>(arg)->PollingTask();
    vTaskDelete(NULL);
}

void Ec11::PollingTask() {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(5));

        int s1 = gpio_get_level(s1_pin_);

        if (s1 != last_s1_) {
            esp_rom_delay_us(1000);
            s1 = gpio_get_level(s1_pin_);
            if (s1 != last_s1_) {
                last_s1_ = s1;
                if (s1 == 0 && rotate_cb_) {
                    int s2 = gpio_get_level(s2_pin_);
                    int direction = (s2 == 0) ? -1 : 1;
                    rotate_cb_(direction);
                }
            }
        }

        bool key = gpio_get_level(key_pin_);

        // Key press detected (falling edge)
        if (!key && last_key_ && !key_handled_) {
            vTaskDelay(pdMS_TO_TICKS(20));
            if (gpio_get_level(key_pin_) == 0) {
                key_handled_ = true;
                if (key_event_cb_) {
                    key_event_cb_(true);
                }
            }
        }

        // Key release detected (rising edge)
        if (key && !last_key_ && key_handled_) {
            key_handled_ = false;
            if (key_event_cb_) {
                key_event_cb_(false);
            }
        }

        last_key_ = key;
    }
}
