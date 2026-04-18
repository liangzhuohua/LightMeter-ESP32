#ifndef APP_UI_WEATHER_PORT_H
#define APP_UI_WEATHER_PORT_H

#include <stdint.h>
#include "app_weather.h"

void app_ui_weather_set_temp(int temp);
void app_ui_weather_set_temp_range(int temp_min, int temp_max);
void app_ui_weather_set_icon(const char* icon_code);
void app_ui_weather_set_humidity(int humidity);
void app_ui_weather_set_wind(float wind_speed);

void app_ui_sunrise_set_time(int hour, int minute);
void app_ui_sunset_set_time(int hour, int minute);
void app_ui_sunrise_sunset_set_now(const char* now_str);
void app_ui_sunrise_sunset_set_indicator(int position_percent);

void app_ui_weather_update_all(const weather_data_t* data);

void app_ui_weather_set_loading(void);
void app_ui_weather_set_success(void);
void app_ui_weather_set_fail(void);

#endif
