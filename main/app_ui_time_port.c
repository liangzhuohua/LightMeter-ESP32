#include "app_ui_time_port.h"
#include "app_controller.h"
#include "lvgl.h"
#include <stdio.h>

extern lv_obj_t *time_time_label;
extern lv_obj_t *time_date_label;
extern lv_obj_t *main_table_time;
extern lv_obj_t *time_status_dot;

static lv_anim_t time_blink_anim;

static void time_dot_anim_cb(void* var, int32_t v) {
    lv_obj_t* dot = (lv_obj_t*)var;
    if (v) {
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    }
}

void app_ui_time_set_loading(void) {
    if (time_status_dot == NULL) return;

    lv_anim_del(time_status_dot, time_dot_anim_cb);
    lv_obj_clear_flag(time_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(time_status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(time_status_dot, lv_color_hex(0xffd700), 0);
}

void app_ui_time_set_success(void) {
    if (time_status_dot == NULL) return;

    lv_anim_del(time_status_dot, time_dot_anim_cb);
    lv_obj_clear_flag(time_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(time_status_dot, lv_color_hex(0x00ff00), 0);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, time_status_dot);
    lv_anim_set_exec_cb(&fade, time_dot_anim_cb);
    lv_anim_set_values(&fade, 1, 0);
    lv_anim_set_time(&fade, 800);
    lv_anim_set_delay(&fade, 1500);
    lv_anim_set_repeat_count(&fade, 0);
    lv_anim_start(&fade);
}

void app_ui_time_set_fail(void) {
    if (time_status_dot == NULL) return;

    lv_anim_del(time_status_dot, time_dot_anim_cb);
    lv_obj_clear_flag(time_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(time_status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(time_status_dot, lv_color_hex(0xff6b6b), 0);
}

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

bool app_ui_time_request_sync(void) {
    return app_controller_request_time_sync();
}
