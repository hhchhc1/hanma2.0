#ifndef WIFI_SETTINGS_H
#define WIFI_SETTINGS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

void wifi_settings_open(const lv_font_t *text_font);
void wifi_settings_close(void);
bool wifi_settings_is_open(void);

#ifdef __cplusplus
}
#endif

#endif
