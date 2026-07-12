#ifndef GPIO_SCCB_H
#define GPIO_SCCB_H

#include "esp_err.h"
#include "esp_sccb_types.h"

esp_err_t gpio_sccb_new_io(int sda_pin, int scl_pin, uint16_t dev_addr, esp_sccb_io_handle_t *io_handle);

esp_err_t gpio_sccb_scan(int sda_pin, int scl_pin);

#endif
