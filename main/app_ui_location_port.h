#ifndef APP_UI_LOCATION_PORT_H
#define APP_UI_LOCATION_PORT_H

#include <stdbool.h>

void app_ui_location_set_city(const char* city);
void app_ui_location_set_detail(const char* detail);
void app_ui_location_set_unknown(void);
void app_ui_location_set_loading(void);
void app_ui_location_set_success(void);
void app_ui_location_set_fail(void);
bool app_ui_location_request_refresh(void);

#endif
