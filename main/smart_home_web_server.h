#ifndef SMART_HOME_WEB_SERVER_H
#define SMART_HOME_WEB_SERVER_H

#include <cstdint>

void smart_home_web_server_start(void);
void smart_home_web_server_configure_ap(void);

void smart_home_get_room2_sensors(float *temp, float *humid, float *light, int64_t *last_update);

#endif
