#ifndef APP_UI_BATTERY_PORT_H
#define APP_UI_BATTERY_PORT_H

#include "app_battery.h"

void app_ui_battery_update(float soc_pct, float voltage_mv, battery_status_t status);

#endif
