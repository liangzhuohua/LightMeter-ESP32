#include "app_ui_weather_port.h"
#include "app_time.h"
#include "lvgl.h"
#include <stdio.h>
#include "esp_err.h"

LV_FONT_DECLARE(qweather_icons);

static const struct {
    int code;
    uint32_t unicode;
} icon_map[] = {
    {100, 0xF101}, {101, 0xF102}, {102, 0xF103}, {103, 0xF104}, {104, 0xF105},
    {150, 0xF106}, {151, 0xF107}, {152, 0xF108}, {153, 0xF109},
    {300, 0xF10A}, {301, 0xF10B}, {302, 0xF10C}, {303, 0xF10D}, {304, 0xF10E},
    {305, 0xF10F}, {306, 0xF110}, {307, 0xF111}, {308, 0xF112}, {309, 0xF113},
    {310, 0xF114}, {311, 0xF115}, {312, 0xF116}, {313, 0xF117}, {314, 0xF118},
    {315, 0xF119}, {316, 0xF11A}, {317, 0xF11B}, {318, 0xF11C}, {350, 0xF11D},
    {351, 0xF11E}, {399, 0xF11F}, {400, 0xF120}, {401, 0xF121}, {402, 0xF122},
    {403, 0xF123}, {404, 0xF124}, {405, 0xF125}, {406, 0xF126}, {407, 0xF127},
    {408, 0xF128}, {409, 0xF129}, {410, 0xF12A}, {456, 0xF12B}, {457, 0xF12C},
    {499, 0xF12D}, {500, 0xF12E}, {501, 0xF12F}, {502, 0xF130}, {503, 0xF131},
    {504, 0xF132}, {507, 0xF133}, {508, 0xF134}, {509, 0xF135}, {510, 0xF136},
    {511, 0xF137}, {512, 0xF138}, {513, 0xF139}, {514, 0xF13A}, {515, 0xF13B},
    {800, 0xF13C}, {801, 0xF13D}, {802, 0xF13E}, {803, 0xF13F}, {804, 0xF140},
    {805, 0xF141}, {806, 0xF142}, {807, 0xF143}, {900, 0xF144}, {901, 0xF145},
    {902, 0xF146}, {903, 0xF147}, {999, 0xF148},
    {-1, 0}
};

static uint32_t get_icon_unicode(int icon_code) {
    for (int i = 0; icon_map[i].code != -1; i++) {
        if (icon_map[i].code == icon_code) {
            return icon_map[i].unicode;
        }
    }
    return 0xF101;
}

extern lv_obj_t* weather_temp_label;
extern lv_obj_t* weather_desc_label;
extern lv_obj_t* humidity_label;
extern lv_obj_t* wind_label;
extern lv_obj_t* sunrise_label;
extern lv_obj_t* sunset_label;
extern lv_obj_t* timeline_indicator;
extern lv_obj_t* timeline_bar;
extern lv_obj_t* weather_icon;

static int g_sunrise_minutes = 0;
static int g_sunset_minutes = 0;
static bool g_sunrise_sunset_valid = false;
static lv_timer_t* g_sun_indicator_timer = NULL;

static void sun_indicator_timer_cb(lv_timer_t* timer) {
    if (!g_sunrise_sunset_valid) return;

    app_time_t now;
    if (app_time_get_now(&now) != ESP_OK) return;

    int now_minutes = now.hour * 60 + now.minute;
    int position_percent = 0;

    if (g_sunset_minutes > g_sunrise_minutes) {
        if (now_minutes <= g_sunrise_minutes) {
            position_percent = 0;
        } else if (now_minutes >= g_sunset_minutes) {
            position_percent = 100;
        } else {
            position_percent = ((now_minutes - g_sunrise_minutes) * 100) / (g_sunset_minutes - g_sunrise_minutes);
        }
    }
    app_ui_sunrise_sunset_set_indicator(position_percent);
}

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
    snprintf(buf, sizeof(buf), LV_SYMBOL_REFRESH " %.1fkm/h", wind_speed);
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

void app_ui_weather_set_icon(const char* icon_code) {
    if (weather_icon == NULL || icon_code == NULL) return;

    int icon_num = atoi(icon_code);
    uint32_t unicode = get_icon_unicode(icon_num);

    char buf[8];
    buf[0] = (char)(0xF0 | ((unicode >> 18) & 0x07));
    buf[1] = (char)(0x80 | ((unicode >> 12) & 0x3F));
    buf[2] = (char)(0x80 | ((unicode >> 6) & 0x3F));
    buf[3] = (char)(0x80 | (unicode & 0x3F));
    buf[4] = '\0';

    lv_label_set_text(weather_icon, buf);
    lv_obj_set_style_text_font(weather_icon, &qweather_icons, 0);
}

void app_ui_weather_update_all(const weather_data_t* data) {
    if (data == NULL) return;

    app_ui_weather_set_temp(data->temp);
    app_ui_weather_set_desc(data->desc);
    app_ui_weather_set_icon(data->icon);
    app_ui_weather_set_humidity(data->humidity);
    app_ui_weather_set_wind(data->wind_speed);
    app_ui_sunrise_set_time(data->sunrise_hour, data->sunrise_minute);
    app_ui_sunset_set_time(data->sunset_hour, data->sunset_minute);

    g_sunrise_minutes = data->sunrise_hour * 60 + data->sunrise_minute;
    g_sunset_minutes = data->sunset_hour * 60 + data->sunset_minute;
    g_sunrise_sunset_valid = true;

    if (g_sun_indicator_timer == NULL) {
        g_sun_indicator_timer = lv_timer_create(sun_indicator_timer_cb, 60000, NULL);
    }

    sun_indicator_timer_cb(NULL);
}
