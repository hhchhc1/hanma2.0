#ifndef __DHT22_H__
#define __DHT22_H__

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile float dht22_temperature;
extern volatile float dht22_humidity;
extern volatile uint8_t dht22_valid;

esp_err_t dht22_init(gpio_num_t gpio_num);
void dht22_start_reading_task(void);

#ifdef __cplusplus
}
#endif

#endif
