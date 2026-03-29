#include "app_controller.h"
#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "app_ui_wifi_port.h"
#include "app_exposure_calc.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "hw_veml7700.h"
#include "hw_oled.h"
#include "hw_wifi.h"

static const char* TAG = "app_controller";

QueueHandle_t lux_value_queue = NULL;
QueueHandle_t calc_data_queue = NULL;
QueueHandle_t wifi_operation_queue = NULL;

static void wifi_state_callback(const hw_wifi_state_event_t *event) {
    if (event == NULL) {
        return;
    }

    if (example_lvgl_lock(-1)) {
        switch (event->state) {
            case HW_WIFI_STATE_CONNECTING:
                app_ui_wifi_on_connecting(event->ssid);
                break;

            case HW_WIFI_STATE_CONNECTED:
                app_ui_wifi_on_connected(event->ssid);
                break;

            case HW_WIFI_STATE_DISCONNECTED:
                app_ui_wifi_on_disconnected(event->ssid);
                break;

            case HW_WIFI_STATE_CONNECT_FAILED:
                app_ui_wifi_on_connect_failed(event->ssid, event->reason);
                break;

            default:
                break;
        }
        example_lvgl_unlock();
    }
}

static void wifi_scan_done_callback(wifi_scan_result_t* result) {
    if (result == NULL || result->count == 0 || result->ap_list == NULL) {
        ESP_LOGW(TAG, "WiFi扫描结果为空");
        return;
    }

    if (example_lvgl_lock(-1)) {
        for (int i = 0; i < result->count; i++) {
            add_wifi_card(result->ap_list[i].ssid, result->ap_list[i].rssi);
        }
        example_lvgl_unlock();
    }

    hw_wifi_scan_result_free(result);
}

static void task_get_lux_value(void* pvParameters) {
    uint32_t als = 0;
    uint32_t white = 0;
    while (1)
    {

        hw_veml7700_get_ambient_light(&als);
        hw_veml7700_get_white_channel(&white);
        xQueueSend(lux_value_queue, &als, pdMS_TO_TICKS(1000));
        // ESP_LOGI(TAG, "ALS: %u lx", als);
        // ESP_LOGI(TAG, "WHITE: %u lx", white);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

static void task_calc_exposure(void* pvParameters) {
    uint32_t lux = 0;
    int iso = 0;
    float ev = 0.0f;
    uint8_t mode = 0;
    CAM cam = {0};
    LEN len = {0};
    ui_calc_data_t calc_data;
    lv_obj_t *cam_card, *len_card;

    while (1)
    {
        // 等待 lux 数据
        if (xQueueReceive(lux_value_queue, &lux, portMAX_DELAY) == pdTRUE)
        {
            ESP_LOGI(TAG, "LUX: %u lx", lux);
        }
        if (example_lvgl_lock(-1))
        {
            ui_calc_port_update_lux_label(main_label_lux_value, lux);
            example_lvgl_unlock();
        }

        // 更新 lux 显示

        // 获取其他参数
        iso = ui_calc_port_get_iso_from_roller(main_roller_iso);
        ev = ui_calc_port_get_ev_from_roller(main_roller_ev);
        mode = ui_calc_port_get_exposure_mode();

        // 提取相机和镜头参数
        cam_card = app_ui_get_cam_selected_card();
        len_card = app_ui_get_len_selected_card();

        if (cam_card != NULL) {
            cam = ui_calc_port_extract_cam_from_card(cam_card);
        } else {
            ESP_LOGD(TAG, "No camera card selected, skip calculation");
            continue;
        }

        if (len_card != NULL) {
            len = ui_calc_port_extract_len_from_card(len_card);
        } else {
            ESP_LOGD(TAG, "No lens card selected, skip calculation");
            continue;
        }

        // 验证提取的参数是否有效
        if (!cam.shutter_stops || cam.shutter_stop_count <= 0 ||
            !len.aperture_stops || len.aperture_stop_count <= 0) {
            ESP_LOGW(TAG, "Invalid camera or lens parameters, skip calculation");
            if (cam.shutter_stops) free(cam.shutter_stops);
            if (len.aperture_stops) free(len.aperture_stops);
            continue;
        }

        ESP_LOGI(TAG, "Mode: %d, ISO: %d, EV: %.1f", mode, iso, ev);

        // 执行曝光计算
        calc_data = ui_calc_port_exposure(lux, iso, ev, mode, cam, len, main_roller_shutter, main_roller_aperture);

        ESP_LOGI(TAG, "Calc shutter: %.3f, aperture: %.1f", calc_data.shutter, calc_data.aperture);


        if (example_lvgl_lock(-1))
        {
            // 直接更新 UI
            ui_calc_port_set_shutter_to_roller(main_roller_shutter, calc_data.shutter, cam);
            ui_calc_port_set_aperture_to_roller(main_roller_aperture, calc_data.aperture, len);
            example_lvgl_unlock();
        }

        // 释放 cam 和 len 中分配的内存
        if (cam.shutter_stops) free(cam.shutter_stops);
        if (len.aperture_stops) free(len.aperture_stops);
    }
}

static void task_wifi_operation(void* pvParameters) {
    WifiOperationMsg msg;

    while (1) {
        if (xQueueReceive(wifi_operation_queue, &msg, portMAX_DELAY) == pdTRUE) {
            switch (msg.op) {
                case WIFI_OP_ENABLE:
                    hw_wifi_init_with_scan_cb(wifi_scan_done_callback);
                    break;

                case WIFI_OP_DISABLE:
                    hw_wifi_deinit();
                    break;

                case WIFI_OP_SCAN:
                    hw_wifi_scan_async();
                    break;

                case WIFI_OP_CONNECT:
                    hw_wifi_connect(msg.ssid, msg.password);
                    break;

                case WIFI_OP_DISCONNECT:
                    hw_wifi_disconnect();
                    break;

                default:
                    ESP_LOGW(TAG, "未知的WiFi操作: %d", msg.op);
                    break;
            }
        }
    }
}



void app_controller_init(void)
{
    lux_value_queue = xQueueCreate(1, sizeof(uint32_t));
    wifi_operation_queue = xQueueCreate(10, sizeof(WifiOperationMsg));
    hw_wifi_register_state_cb(wifi_state_callback);

    xTaskCreate(task_get_lux_value, "task_get_lux_value", 2048, NULL, 5, NULL);
    xTaskCreate(task_calc_exposure, "task_calc_exposure", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(task_wifi_operation, "task_wifi_operation", 4096, NULL, 5, NULL, 0);
}
