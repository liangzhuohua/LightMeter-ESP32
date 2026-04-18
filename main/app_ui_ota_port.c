#include "app_ui_ota_port.h"
#include "app_controller.h"
#include "app_ui.h"
#include "lvgl.h"
#include <stdio.h>

extern lv_obj_t* ota_win;
extern lv_obj_t* ota_progress_bar;
extern lv_obj_t* ota_status_label;
extern lv_obj_t* ota_ssid_label;
extern lv_obj_t* ota_cancel_btn;

void app_ui_ota_show_window(void) {
    if (ota_win == NULL) return;
    lv_obj_clear_flag(ota_win, LV_OBJ_FLAG_HIDDEN);
}

void app_ui_ota_hide_window(void) {
    if (ota_win == NULL) return;
    lv_obj_add_flag(ota_win, LV_OBJ_FLAG_HIDDEN);
}

void app_ui_ota_set_state(int state, int progress) {
    if (ota_win == NULL) return;

    if (ota_progress_bar) {
        lv_bar_set_value(ota_progress_bar, progress, LV_ANIM_ON);
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
                lv_label_set_text(ota_status_label, "AP Ready\nConnect WiFi: ESP32S3_OTA\nPass: 12345678\nOpen: 192.168.4.1");
                break;
            case 3: {
                char buf[64];
                snprintf(buf, sizeof(buf), "Uploading... %d%%", progress);
                lv_label_set_text(ota_status_label, buf);
                break;
            }
            case 4:
                lv_label_set_text(ota_status_label, "Verifying...");
                break;
            case 5:
                lv_label_set_text(ota_status_label, "Success! Rebooting...");
                if (ota_cancel_btn) {
                    lv_obj_add_flag(ota_cancel_btn, LV_OBJ_FLAG_HIDDEN);
                }
                break;
            case 6:
                lv_label_set_text(ota_status_label, "Failed!");
                break;
            default:
                break;
        }
    }
}

bool app_ui_ota_request_start(void) {
    return app_controller_request_ota();
}

void app_ui_ota_request_cancel(void) {
    app_controller_cancel_ota();
}
