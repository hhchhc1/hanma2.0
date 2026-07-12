#ifndef CAMERA_DISPLAY_H
#define CAMERA_DISPLAY_H

#include "lvgl.h"
#include <driver/i2c_master.h>
#include "esp_err.h"

void camera_display_set_i2c_bus(i2c_master_bus_handle_t bus);
lv_obj_t* camera_display_create(lv_obj_t *parent);
void camera_display_start(void);

esp_err_t camera_get_frame(uint8_t *out_buf, size_t buf_size, int *width, int *height);

void camera_display_set_thermal_effect(bool on);
bool camera_display_get_thermal_effect(void);

extern int s_detection_x;
extern int s_detection_y;
extern int s_detection_w;
extern int s_detection_h;
extern bool s_detection_valid;
extern int64_t s_last_detection_us;

#endif
