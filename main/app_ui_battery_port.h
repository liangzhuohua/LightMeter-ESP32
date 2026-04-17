#ifndef APP_UI_BATTERY_PORT_H
#define APP_UI_BATTERY_PORT_H

#include <stdint.h>
#include <stdbool.h>

void app_ui_battery_update(float soc_pct, float voltage_mv, bool charging);

#endif
