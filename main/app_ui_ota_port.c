#include "app_ui_ota_port.h"
#include "app_controller.h"
#include "app_ui.h"
#include "lvgl.h"
#include <stdio.h>

extern lv_obj_t* ota_win;
extern lv_obj_t* ota_mask;
extern lv_obj_t* ota_progress_bar;
extern lv_obj_t* ota_status_label;
extern lv_obj_t* ota_cancel_btn;
extern lv_obj_t* ota_percent_label;

/* 显示OTA升级窗口 */
void app_ui_ota_show_window(void) {
    if (ota_mask == NULL) return;
    lv_obj_clear_flag(ota_mask, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(ota_win, LV_OBJ_FLAG_HIDDEN);
}

/* 隐藏OTA升级窗口 */
void app_ui_ota_hide_window(void) {
    if (ota_mask == NULL) return;
    lv_obj_add_flag(ota_win, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(ota_mask, LV_OBJ_FLAG_HIDDEN);
}

/**
 * @brief 更新OTA窗口状态和进度显示
 * @param state OTA状态（IDLE/AP_READY/UPLOADING/VERIFYING/SUCCESS/FAIL）
 * @param progress 上传进度百分比(0-100)
 */
void app_ui_ota_set_state(int state, int progress) {
    if (ota_win == NULL) return;

    if (state >= 3) {
        if (ota_progress_bar) {
            lv_obj_clear_flag(ota_progress_bar, LV_OBJ_FLAG_HIDDEN);
            lv_bar_set_value(ota_progress_bar, progress, LV_ANIM_ON);
        }
        if (ota_percent_label) {
            lv_obj_clear_flag(ota_percent_label, LV_OBJ_FLAG_HIDDEN);
            char buf[8];
            snprintf(buf, sizeof(buf), "%d%%", progress);
            lv_label_set_text(ota_percent_label, buf);
        }
    }

    if (ota_status_label) {
        switch (state) {
            case 0:
                lv_label_set_text(ota_status_label, "Idle");
                break;
            case 1:
                lv_label_set_text(ota_status_label, "Starting AP...");
                break;
            case 2:
                lv_label_set_text(ota_status_label, "AP Ready\nConnect phone to WiFi\nOpen 192.168.4.1 in browser");
                break;
            case 3:
                lv_label_set_text(ota_status_label, "Uploading...");
                break;
            case 4:
                lv_label_set_text(ota_status_label, "Verifying firmware...");
                break;
            case 5:
                lv_label_set_text(ota_status_label, LV_SYMBOL_OK " Success! Rebooting...");
                if (ota_cancel_btn) {
                    lv_obj_add_flag(ota_cancel_btn, LV_OBJ_FLAG_HIDDEN);
                }
                if (ota_percent_label) {
                    lv_obj_set_style_text_color(ota_percent_label, lv_color_hex(0x87ceeb), 0);
                }
                if (ota_progress_bar) {
                    lv_obj_set_style_bg_color(ota_progress_bar, lv_color_hex(0x00ff00), LV_PART_INDICATOR);
                }
                break;
            case 6:
                lv_label_set_text(ota_status_label, LV_SYMBOL_CLOSE " Upgrade failed!");
                if (ota_percent_label) {
                    lv_obj_set_style_text_color(ota_percent_label, lv_color_hex(0xff6b6b), 0);
                }
                if (ota_progress_bar) {
                    lv_obj_set_style_bg_color(ota_progress_bar, lv_color_hex(0xff6b6b), LV_PART_INDICATOR);
                }
                break;
            default:
                break;
        }
    }
}

/* 请求启动OTA升级（开启AP热点模式） */
bool app_ui_ota_request_start(void) {
    return app_controller_request_ota();
}

/* 取消OTA升级（关闭AP，恢复STA模式） */
void app_ui_ota_request_cancel(void) {
    app_controller_cancel_ota();
}
