#include "app_ui_location_port.h"
#include "app_controller.h"
#include "lvgl.h"

extern lv_obj_t *location_city_label;
extern lv_obj_t *location_detail_label;
extern lv_obj_t *location_status_dot;

static lv_anim_t location_blink_anim;

static void location_dot_anim_cb(void* var, int32_t v) {
    lv_obj_t* dot = (lv_obj_t*)var;
    if (v) {
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    }
}

void app_ui_location_set_loading(void) {
    if (location_status_dot == NULL) return;

    lv_obj_clear_flag(location_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(location_status_dot, lv_color_hex(0xffd700), 0);

    lv_anim_init(&location_blink_anim);
    lv_anim_set_var(&location_blink_anim, location_status_dot);
    lv_anim_set_exec_cb(&location_blink_anim, location_dot_anim_cb);
    lv_anim_set_values(&location_blink_anim, 0, 1);
    lv_anim_set_time(&location_blink_anim, 500);
    lv_anim_set_playback_time(&location_blink_anim, 500);
    lv_anim_set_repeat_count(&location_blink_anim, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&location_blink_anim);
}

void app_ui_location_set_success(void) {
    if (location_status_dot == NULL) return;

    lv_anim_del(location_status_dot, location_dot_anim_cb);
    lv_obj_clear_flag(location_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(location_status_dot, lv_color_hex(0x00ff00), 0);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, location_status_dot);
    lv_anim_set_exec_cb(&fade, location_dot_anim_cb);
    lv_anim_set_values(&fade, 1, 0);
    lv_anim_set_time(&fade, 3000);
    lv_anim_set_delay(&fade, 2000);
    lv_anim_set_repeat_count(&fade, 0);
    lv_anim_set_ready_cb(&fade, NULL);
    lv_anim_start(&fade);
}

void app_ui_location_set_fail(void) {
    if (location_status_dot == NULL) return;

    lv_anim_del(location_status_dot, location_dot_anim_cb);
    lv_obj_clear_flag(location_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_color(location_status_dot, lv_color_hex(0xff6b6b), 0);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, location_status_dot);
    lv_anim_set_exec_cb(&fade, location_dot_anim_cb);
    lv_anim_set_values(&fade, 1, 0);
    lv_anim_set_time(&fade, 3000);
    lv_anim_set_delay(&fade, 3000);
    lv_anim_set_repeat_count(&fade, 0);
    lv_anim_start(&fade);
}

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

bool app_ui_location_request_refresh(void) {
    return app_controller_request_location();
}
