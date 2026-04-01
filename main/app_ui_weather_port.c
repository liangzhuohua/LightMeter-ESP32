#include "app_ui_weather_port.h"
#include "lvgl.h"
#include <stdio.h>

extern lv_obj_t* weather_temp_label;
extern lv_obj_t* weather_desc_label;
extern lv_obj_t* humidity_label;
extern lv_obj_t* wind_label;
extern lv_obj_t* sunrise_label;
extern lv_obj_t* sunset_label;
extern lv_obj_t* timeline_indicator;
extern lv_obj_t* timeline_bar;

void app_ui_weather_set_temp(int temp) {
    if (weather_temp_label == NULL) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%d°C", temp);
    lv_label_set_text(weather_temp_label, buf);
}

void app_ui_weather_set_desc(const char* desc) {
    if (weather_desc_label == NULL || desc == NULL) return;
    lv_label_set_text(weather_desc_label, desc);
}

void app_ui_weather_set_humidity(int humidity) {
    if (humidity_label == NULL) return;

    char buf[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_TINT " %d%%", humidity);
    lv_label_set_text(humidity_label, buf);
}

void app_ui_weather_set_wind(float wind_speed) {
    if (wind_label == NULL) return;

    char buf[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_REFRESH " %.1fm/s", wind_speed);
    lv_label_set_text(wind_label, buf);
}

void app_ui_sunrise_set_time(int hour, int minute) {
    if (sunrise_label == NULL) return;

    char buf[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_UP " %02d:%02d", hour, minute);
    lv_label_set_text(sunrise_label, buf);
}

void app_ui_sunset_set_time(int hour, int minute) {
    if (sunset_label == NULL) return;

    char buf[16];
    snprintf(buf, sizeof(buf), LV_SYMBOL_DOWN " %02d:%02d", hour, minute);
    lv_label_set_text(sunset_label, buf);
}

void app_ui_sunrise_sunset_set_now(const char* now_str) {
    return;
}

void app_ui_sunrise_sunset_set_indicator(int position_percent) {
    if (timeline_indicator == NULL || timeline_bar == NULL) return;

    if (position_percent < 0) position_percent = 0;
    if (position_percent > 100) position_percent = 100;

    lv_obj_update_layout(timeline_bar);
    int bar_width = lv_obj_get_width(timeline_bar);
    int indicator_offset = (bar_width * position_percent) / 100 - 6;

    if (indicator_offset < 0) indicator_offset = 0;

    lv_obj_align(timeline_indicator, LV_ALIGN_LEFT_MID, indicator_offset, 0);
}
