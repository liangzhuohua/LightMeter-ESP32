#include "app_ui_location_port.h"
#include "app_controller.h"
#include "lvgl.h"

extern lv_obj_t *location_city_label;
extern lv_obj_t *location_detail_label;
extern lv_obj_t *location_status_dot;

static lv_anim_t location_blink_anim;

/* 定位状态点的闪烁动画回调 */
static void location_dot_anim_cb(void* var, int32_t v) {
    lv_obj_t* dot = (lv_obj_t*)var;
    if (v) {
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(dot, LV_OBJ_FLAG_HIDDEN);
    }
}

/* 设置定位状态为加载中（黄色点） */
void app_ui_location_set_loading(void) {
    if (location_status_dot == NULL) return;

    lv_anim_del(location_status_dot, location_dot_anim_cb);
    lv_obj_clear_flag(location_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(location_status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(location_status_dot, lv_color_hex(0xffd700), 0);
}

/* 设置定位状态为成功（绿色点，1.5秒后淡出） */
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
    lv_anim_set_time(&fade, 800);
    lv_anim_set_delay(&fade, 1500);
    lv_anim_set_repeat_count(&fade, 0);
    lv_anim_start(&fade);
}

/* 设置定位状态为失败（红色点） */
void app_ui_location_set_fail(void) {
    if (location_status_dot == NULL) return;

    lv_anim_del(location_status_dot, location_dot_anim_cb);
    lv_obj_clear_flag(location_status_dot, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_bg_opa(location_status_dot, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(location_status_dot, lv_color_hex(0xff6b6b), 0);
}

/* 设置定位城市名称显示 */
void app_ui_location_set_city(const char* city) {
    if (location_city_label == NULL || city == NULL) return;

    lv_label_set_text(location_city_label, city);
    lv_obj_set_style_text_color(location_city_label, lv_color_white(), 0);
}

/* 设置定位详情（街道/区）显示 */
void app_ui_location_set_detail(const char* detail) {
    if (location_detail_label == NULL || detail == NULL) return;

    lv_label_set_text(location_detail_label, detail);
}

/* 设置定位状态为未知（灰色"未知"文字） */
void app_ui_location_set_unknown(void) {
    if (location_city_label == NULL) return;

    lv_label_set_text(location_city_label, "未知");
    lv_obj_set_style_text_color(location_city_label, lv_color_hex(0x888888), 0);

    if (location_detail_label != NULL) {
        lv_label_set_text(location_detail_label, "未定位");
    }
}

/* 请求手动刷新定位 */
bool app_ui_location_request_refresh(void) {
    return app_controller_request_location();
}
