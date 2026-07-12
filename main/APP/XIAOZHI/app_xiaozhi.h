#ifndef __APP_XIAOZHI_H__
#define __APP_XIAOZHI_H__

#include "lvgl.h"

void app_xiaozhi_init(lv_obj_t *parent);
void xiaozhi_add_message(const char *role, const char *text);
void xiaozhi_set_status(const char *status);
void xiaozhi_set_wifi(const char *ssid);

#endif
