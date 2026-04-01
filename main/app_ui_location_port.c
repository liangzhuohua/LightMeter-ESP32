#include "app_ui_location_port.h"
#include "lvgl.h"

extern lv_obj_t *location_city_label;
extern lv_obj_t *location_detail_label;

void app_ui_location_set_city(const char* city) {
    if (location_city_label == NULL || city == NULL) return;

    lv_label_set_text(location_city_label, city);
    lv_obj_set_style_text_color(location_city_label, lv_color_white(), 0);
}

void app_ui_location_set_detail(const char* detail) {
    if (location_detail_label == NULL || detail == NULL) return;

    lv_label_set_text(location_detail_label, detail);
}

void app_ui_location_set_unknown(void) {
    if (location_city_label == NULL) return;

    lv_label_set_text(location_city_label, "未知");
    lv_obj_set_style_text_color(location_city_label, lv_color_hex(0x888888), 0);

    if (location_detail_label != NULL) {
        lv_label_set_text(location_detail_label, "未定位");
    }
}
