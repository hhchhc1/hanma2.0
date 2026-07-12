#ifndef __XIAOZHI_ADAPTER_H__
#define __XIAOZHI_ADAPTER_H__

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*xiaozhi_msg_cb_t)(const char *role, const char *text);
typedef void (*xiaozhi_status_cb_t)(const char *status);
typedef void (*xiaozhi_wifi_cb_t)(const char *ssid);

esp_err_t xiaozhi_adapter_init(const char *ws_url, const char *ws_token);
void xiaozhi_adapter_toggle_chat(void);
void xiaozhi_adapter_set_message_callback(xiaozhi_msg_cb_t cb);
void xiaozhi_adapter_set_status_callback(xiaozhi_status_cb_t cb);
void xiaozhi_adapter_set_wifi_callback(xiaozhi_wifi_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif
