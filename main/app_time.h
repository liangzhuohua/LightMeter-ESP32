#ifndef __APP_TIME_H__
#define __APP_TIME_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int weekday;
} app_time_t;

esp_err_t app_time_sntp_init(void);
esp_err_t app_time_sntp_sync(void);
bool app_time_is_synced(void);
esp_err_t app_time_get_now(app_time_t* time);
void app_time_wait_sync(uint32_t timeout_ms);
esp_err_t app_time_save_to_rtc(void);
esp_err_t app_time_restore_from_rtc(void);
bool app_time_has_rtc_backup(void);
void app_time_set_timezone(double longitude);

#endif
