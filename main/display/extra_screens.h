#ifndef EXTRA_SCREENS_H
#define EXTRA_SCREENS_H

#include <lvgl.h>
#include <cstdint>

#define EXTRA_TILE_COUNT 7

void extra_screens_create(lv_obj_t *tv);
void extra_screens_update_time(void);
void extra_screens_update_sensor_data(void);
void extra_screens_update_timer(void);

const char* get_weather_data(void);

#ifdef __cplusplus
extern "C" {
#endif

bool smart_home_get_fan_state(void);
bool smart_home_get_light_state(void);
bool smart_home_get_auto_mode(void);
void smart_home_set_fan_state(bool on);
void smart_home_set_light_state(bool on);
void smart_home_set_auto_mode(bool auto_mode);

#ifdef __cplusplus
}
#endif

#endif
