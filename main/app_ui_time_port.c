#include "app_ui_time_port.h"
#include "lvgl.h"
#include <stdio.h>

extern lv_obj_t *time_time_label;
extern lv_obj_t *time_date_label;
extern lv_obj_t *main_table_time;

void app_ui_time_set_time(int hour, int minute) {
    if (time_time_label == NULL) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d", hour, minute);
    lv_label_set_text(time_time_label, buf);
}

void app_ui_time_set_date(int year, int month, int day) {
    if (time_date_label == NULL) return;

    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d", year, month, day);
    lv_label_set_text(time_date_label, buf);
}

void app_ui_time_set_main_table_time(int hour, int minute) {
    if (main_table_time == NULL) return;

    const char* ampm = (hour < 12) ? "AM" : "PM";
    int display_hour = (hour % 12 == 0) ? 12 : hour % 12;

    char buf[16];
    snprintf(buf, sizeof(buf), "%s %d:%02d", ampm, display_hour, minute);
    lv_label_set_text(main_table_time, buf);
}
