#ifndef APP_UI_WEATHER_PORT_H
#define APP_UI_WEATHER_PORT_H

#include <stdint.h>

void app_ui_weather_set_temp(int temp);
void app_ui_weather_set_desc(const char* desc);
void app_ui_weather_set_humidity(int humidity);
void app_ui_weather_set_wind(float wind_speed);

void app_ui_sunrise_set_time(int hour, int minute);
void app_ui_sunset_set_time(int hour, int minute);
void app_ui_sunrise_sunset_set_now(const char* now_str);
void app_ui_sunrise_sunset_set_indicator(int position_percent);

#endif
