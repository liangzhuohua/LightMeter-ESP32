#include "app_ui_wifi_port.h"
#include "app_ui.h"
#include "app_controller.h"
#include "hw_oled.h"
#include <string.h>

void ui_wifi_port_wifi_enable(void) {
    WifiOperationMsg msg = { .op = WIFI_OP_ENABLE };
    xQueueSend(wifi_operation_queue, &msg, pdMS_TO_TICKS(1000));
}

void ui_wifi_port_wifi_disable(void) {
    WifiOperationMsg msg = { .op = WIFI_OP_DISABLE };
    xQueueSend(wifi_operation_queue, &msg, pdMS_TO_TICKS(1000));
}

void ui_wifi_port_wifi_scan(void) {
    WifiOperationMsg msg = { .op = WIFI_OP_SCAN };
    xQueueSend(wifi_operation_queue, &msg, pdMS_TO_TICKS(1000));
}

void ui_wifi_port_wifi_connect(const char *ssid, const char *password) {
    WifiOperationMsg msg = { .op = WIFI_OP_CONNECT };
    if (ssid) {
        strncpy(msg.ssid, ssid, sizeof(msg.ssid) - 1);
        msg.ssid[sizeof(msg.ssid) - 1] = '\0';
    } else {
        msg.ssid[0] = '\0';
    }
    if (password) {
        strncpy(msg.password, password, sizeof(msg.password) - 1);
        msg.password[sizeof(msg.password) - 1] = '\0';
    } else {
        msg.password[0] = '\0';
    }
    xQueueSend(wifi_operation_queue, &msg, pdMS_TO_TICKS(1000));
}

void ui_wifi_port_wifi_disconnect(void) {
    WifiOperationMsg msg = { .op = WIFI_OP_DISCONNECT };
    xQueueSend(wifi_operation_queue, &msg, pdMS_TO_TICKS(1000));
}

void ui_wifi_port_add_wifi_card(const char *wifi_name, int signal_strength) {
    if (wifi_name == NULL || strlen(wifi_name) == 0) {
        return;
    }

    if (signal_strength < -100) {
        return;
    }

    // if (example_lvgl_lock(-1)) {
        add_wifi_card(wifi_name, signal_strength);
    //     example_lvgl_unlock();
    // }
}

void ui_wifi_port_set_connected(const char *ssid) {
    extern lv_obj_t *wifi_icon_label;
    extern lv_obj_t *wifi_ssid_label;
    extern lv_obj_t *wifi_status_label;

    if (wifi_icon_label != NULL) {
        lv_obj_set_style_text_color(wifi_icon_label, lv_color_hex(0x00ff00), 0);
    }

    if (wifi_ssid_label != NULL && ssid != NULL) {
        lv_label_set_text(wifi_ssid_label, ssid);
        lv_obj_set_style_text_color(wifi_ssid_label, lv_color_white(), 0);
    }

    if (wifi_status_label != NULL) {
        lv_label_set_text(wifi_status_label, "已连接");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0x00ff00), 0);
    }
}

void ui_wifi_port_set_disconnected(void) {
    extern lv_obj_t *wifi_icon_label;
    extern lv_obj_t *wifi_ssid_label;
    extern lv_obj_t *wifi_status_label;

    if (wifi_icon_label != NULL) {
        lv_obj_set_style_text_color(wifi_icon_label, lv_color_hex(0x888888), 0);
    }

    if (wifi_ssid_label != NULL) {
        lv_label_set_text(wifi_ssid_label, "未连接");
        lv_obj_set_style_text_color(wifi_ssid_label, lv_color_hex(0x888888), 0);
    }

    if (wifi_status_label != NULL) {
        lv_label_set_text(wifi_status_label, "已断开");
        lv_obj_set_style_text_color(wifi_status_label, lv_color_hex(0xff6b6b), 0);
    }
}
