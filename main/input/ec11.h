#ifndef EC11_H
#define EC11_H

#include <driver/gpio.h>
#include <functional>

class Ec11 {
public:
    static Ec11& GetInstance();

    void Init(gpio_num_t s1_pin, gpio_num_t s2_pin, gpio_num_t key_pin);

    using RotateCallback = std::function<void(int direction)>;
    using KeyEventCallback = std::function<void(bool is_press)>;

    void OnRotate(RotateCallback cb) { rotate_cb_ = std::move(cb); }
    void OnKeyEvent(KeyEventCallback cb) { key_event_cb_ = std::move(cb); }

private:
    Ec11() = default;
    ~Ec11();

    static void PollingTaskWrapper(void* arg);
    void PollingTask();

    gpio_num_t s1_pin_ = GPIO_NUM_NC;
    gpio_num_t s2_pin_ = GPIO_NUM_NC;
    gpio_num_t key_pin_ = GPIO_NUM_NC;

    int last_s1_ = 1;
    bool last_key_ = true;
    bool key_handled_ = false;

    RotateCallback rotate_cb_;
    KeyEventCallback key_event_cb_;
};

#endif
