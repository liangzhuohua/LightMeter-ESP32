#ifndef APP_UI_TIME_PORT_H
#define APP_UI_TIME_PORT_H

#include <stdint.h>

void app_ui_time_set_time(int hour, int minute);
void app_ui_time_set_date(int year, int month, int day);
void app_ui_time_set_main_table_time(int hour, int minute);
void app_ui_time_request_sync(void);

#endif
