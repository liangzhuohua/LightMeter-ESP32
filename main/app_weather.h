#ifndef __APP_WEATHER_H__
#define __APP_WEATHER_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    int temp;
    char desc[32];
    char icon[8];
    int humidity;
    float wind_speed;
    int sunrise_hour;
    int sunrise_minute;
    int sunset_hour;
    int sunset_minute;
} weather_data_t;

typedef void (*weather_result_callback_t)(const weather_data_t* data);

void app_weather_init(void);
void app_weather_get(const char* location_id, weather_result_callback_t callback);

#endif
