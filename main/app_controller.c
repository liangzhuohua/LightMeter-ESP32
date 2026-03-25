#include "app_controller.h"
#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "app_exposure_calc.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "hw_veml7700.h"
#include "hw_oled.h"


static const char* TAG = "app_controller";

QueueHandle_t lux_value_queue = NULL;
QueueHandle_t calc_data_queue = NULL;

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

static void task_flash_ui(void* pvParameters) {
    // 此任务已废弃，UI 更新在 task_calc_exposure 中直接进行
    vTaskDelete(NULL);
}

void app_controller_init(void)
{
    lux_value_queue = xQueueCreate(1, sizeof(uint32_t));
    
    xTaskCreate(task_get_lux_value, "task_get_lux_value", 2048, NULL, 5, NULL);
    xTaskCreate(task_calc_exposure, "task_calc_exposure", 4096, NULL, 5, NULL);
}