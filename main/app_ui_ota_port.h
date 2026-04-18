#ifndef APP_UI_OTA_PORT_H
#define APP_UI_OTA_PORT_H

#include <stdbool.h>

void app_ui_ota_show_window(void);
void app_ui_ota_hide_window(void);
void app_ui_ota_set_state(int state, int progress);
bool app_ui_ota_request_start(void);
void app_ui_ota_request_cancel(void);

#endif
