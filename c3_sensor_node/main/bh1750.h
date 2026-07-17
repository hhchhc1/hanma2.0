#ifndef __BH1750_H__
#define __BH1750_H__

#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile float bh1750_lux;
extern volatile uint8_t bh1750_valid;

esp_err_t bh1750_init(int sda_gpio, int scl_gpio);
void bh1750_start_reading_task(void);

#ifdef __cplusplus
}
#endif

#endif
