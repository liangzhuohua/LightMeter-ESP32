#ifndef __APP_BATTERY_H__
#define __APP_BATTERY_H__

#include <stdint.h>

typedef enum {
    BATTERY_STATUS_DISCHARGING = 0,
    BATTERY_STATUS_CHARGING,
    BATTERY_STATUS_FULL,
} battery_status_t;

void app_battery_init(void);
void app_battery_get_info(float *soc_pct, float *voltage_mv, battery_status_t *status);
void app_battery_sleep(void);

#endif
