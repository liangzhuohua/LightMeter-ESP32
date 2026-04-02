#include "app_controller.h"
#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "app_ui_wifi_port.h"
#include "app_ui_location_port.h"
#include "app_ui_time_port.h"
#include "app_exposure_calc.h"
#include "app_time.h"
#include "app_weather.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "hw_veml7700.h"
#include "hw_oled.h"
#include "hw_wifi.h"
#include "app_location.h"

static const char* TAG = "app_controller";

QueueHandle_t lux_value_queue = NULL;
QueueHandle_t calc_data_queue = NULL;
QueueHandle_t wifi_operation_queue = NULL;
SemaphoreHandle_t location_Sem = NULL;
SemaphoreHandle_t time_sync_Sem = NULL;
SemaphoreHandle_t weather_Sem = NULL;

static wifi_scan_result_t g_wifi_scan_result = {0};
static bool g_wifi_connected = false;
static bool g_wifi_scanned = false;
static bool g_time_synced = false;

static void weather_result_callback(const weather_data_t* data);

static void wifi_state_callback(const hw_wifi_state_event_t *event) {
    if (event == NULL) {
        return;
    }

    if (example_lvgl_lock(-1)) {
        switch (event->state) {
            case HW_WIFI_STATE_CONNECTING:
                app_ui_wifi_on_connecting(event->ssid);
                g_wifi_connected = false;
                break;

            case HW_WIFI_STATE_CONNECTED:
                app_ui_wifi_on_connected(event->ssid);
                g_wifi_connected = true;
                break;

            case HW_WIFI_STATE_DISCONNECTED:
                app_ui_wifi_on_disconnected(event->ssid);
                g_wifi_connected = false;
                break;

            case HW_WIFI_STATE_CONNECT_FAILED:
                app_ui_wifi_on_connect_failed(event->ssid, event->reason);
                g_wifi_connected = false;
                break;

            default:
                break;
        }
        example_lvgl_unlock();
    }

    if (event->state == HW_WIFI_STATE_CONNECTED && g_wifi_scanned) {
        ESP_LOGI(TAG, "WiFi已连接，自动触发定位");
        xSemaphoreGive(location_Sem);
    }

    if (event->state == HW_WIFI_STATE_CONNECTED) {
        ESP_LOGI(TAG, "WiFi已连接，触发时间同步");
        app_time_sntp_init();
        xSemaphoreGive(time_sync_Sem);

        ESP_LOGI(TAG, "WiFi已连接，触发天气获取");
        xSemaphoreGive(weather_Sem);
    }
}

static void wifi_scan_done_callback(wifi_scan_result_t* result) {
    if (result == NULL || result->count == 0 || result->ap_list == NULL) {
        ESP_LOGW(TAG, "WiFi扫描结果为空");
        g_wifi_scanned = false;
        return;
    }

    // 释放之前保存的扫描结果
    if (g_wifi_scan_result.ap_list != NULL) {
        free(g_wifi_scan_result.ap_list);
        g_wifi_scan_result.ap_list = NULL;
        g_wifi_scan_result.count = 0;
    }

    // 保存新的扫描结果
    g_wifi_scan_result.ap_list = malloc(sizeof(wifi_info_t) * result->count);
    if (g_wifi_scan_result.ap_list != NULL) {
        memcpy(g_wifi_scan_result.ap_list, result->ap_list, sizeof(wifi_info_t) * result->count);
        g_wifi_scan_result.count = result->count;
        g_wifi_scanned = true;
        ESP_LOGI(TAG, "已保存 %d 个WiFi扫描结果", result->count);
    } else {
        ESP_LOGE(TAG, "保存WiFi扫描结果失败：内存不足");
        g_wifi_scanned = false;
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

static void location_result_callback(const location_result_t* result) {
    if (result == NULL) {
        ESP_LOGE(TAG, "定位结果为空");
        app_ui_location_set_unknown();
        return;
    }

    ESP_LOGI(TAG, "定位结果回调:");
    ESP_LOGI(TAG, "  纬度: %f", result->latitude);
    ESP_LOGI(TAG, "  经度: %f", result->longitude);
    ESP_LOGI(TAG, "  精度: %f 米", result->accuracy);
    ESP_LOGI(TAG, "  地址: %s", result->address);

    char city[64] = {0};
    char detail[128] = {0};

    const char* addr = result->address;
    if (addr != NULL && addr[0] != '\0') {
        const char* province_end = strstr(addr, "省");
        if (province_end != NULL) {
            province_end += 3;
        } else {
            province_end = addr;
        }

        const char* city_end = strstr(province_end, "市");
        if (city_end != NULL) {
            int city_len = city_end - province_end;
            if (city_len > 0 && city_len < sizeof(city)) {
                strncpy(city, province_end, city_len);
                city[city_len] = '\0';
            }

            const char* detail_start = city_end + 3;
            if (detail_start[0] != '\0') {
                const char* detail_end = strstr(detail_start, ";");
                if (detail_end == NULL) {
                    detail_end = strstr(detail_start, "\n");
                }
                if (detail_end == NULL) {
                    detail_end = detail_start + strlen(detail_start);
                }
                int detail_len = detail_end - detail_start;
                if (detail_len > 0 && detail_len < sizeof(detail)) {
                    strncpy(detail, detail_start, detail_len);
                    detail[detail_len] = '\0';
                }
            }
        } else {
            strncpy(city, province_end, sizeof(city) - 1);
        }
    }

    if (city[0] != '\0') {
        app_ui_location_set_city(city);
    } else {
        app_ui_location_set_city("Unknown");
    }

    if (detail[0] != '\0') {
        app_ui_location_set_detail(detail);
    } else {
        app_ui_location_set_detail(result->address);
    }
}

static void weather_result_callback(const weather_data_t* data) {
    if (data == NULL) {
        ESP_LOGE(TAG, "天气结果为空");
        return;
    }

    ESP_LOGI(TAG, "========== 天气测试结果 ==========");
    ESP_LOGI(TAG, "  温度: %d°C", data->temp);
    ESP_LOGI(TAG, "  天气: %s", data->desc);
    ESP_LOGI(TAG, "  湿度: %d%%", data->humidity);
    ESP_LOGI(TAG, "  风速: %.1f km/h", data->wind_speed);
    ESP_LOGI(TAG, "  日出: %02d:%02d", data->sunrise_hour, data->sunrise_minute);
    ESP_LOGI(TAG, "  日落: %02d:%02d", data->sunset_hour, data->sunset_minute);
    ESP_LOGI(TAG, "==================================");
}

static void task_get_location(void* pvParameters) {
    while (1) {
        // 等待定位请求消息
        if (xSemaphoreTake(location_Sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "收到定位请求");

            // 检查条件1：是否已扫描WiFi
            if (!g_wifi_scanned) {
                ESP_LOGW(TAG, "定位失败：未扫描WiFi");
                continue;
            }

            // 检查条件2：是否已连接WiFi
            if (!g_wifi_connected) {
                ESP_LOGW(TAG, "定位失败：未连接WiFi");
                continue;
            }

            // 检查扫描结果是否有效
            if (g_wifi_scan_result.count == 0 || g_wifi_scan_result.ap_list == NULL) {
                ESP_LOGW(TAG, "定位失败：扫描结果为空");
                continue;
            }

            ESP_LOGI(TAG, "开始定位，WiFi数量: %d", g_wifi_scan_result.count);

            // 调用定位函数
            app_location_get_location(&g_wifi_scan_result, location_result_callback);
        }
    }
}

static void task_time_sync_and_update(void* pvParameters) {
    app_time_t current_time;

    while (1) {
        if (xSemaphoreTake(time_sync_Sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "收到时间同步请求");

            if (!g_wifi_connected) {
                ESP_LOGW(TAG, "时间同步失败：未连接WiFi");
                continue;
            }

            if (!app_time_is_synced()) {
                ESP_LOGI(TAG, "开始同步时间");
                app_time_sntp_sync();
                app_time_wait_sync(10000);
            }

            if (app_time_is_synced()) {
                g_time_synced = true;
                ESP_LOGI(TAG, "时间同步成功");

                if (app_time_get_now(&current_time) == ESP_OK) {
                    ESP_LOGI(TAG, "当前时间: %04d-%02d-%02d %02d:%02d:%02d",
                             current_time.year, current_time.month, current_time.day,
                             current_time.hour, current_time.minute, current_time.second);

                    if (example_lvgl_lock(-1)) {
                        app_ui_time_set_time(current_time.hour, current_time.minute);
                        app_ui_time_set_date(current_time.year, current_time.month, current_time.day);
                        app_ui_time_set_main_table_time(current_time.hour, current_time.minute);
                        example_lvgl_unlock();
                    }
                }
            } else {
                ESP_LOGW(TAG, "时间同步失败");
            }
        }
    }
}

static void task_time_update_periodic(void* pvParameters) {
    app_time_t current_time;
    int last_minute = -1;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

        if (!g_time_synced) {
            continue;
        }

        if (app_time_get_now(&current_time) == ESP_OK) {
            if (current_time.minute != last_minute) {
                last_minute = current_time.minute;

                if (example_lvgl_lock(-1)) {
                    app_ui_time_set_time(current_time.hour, current_time.minute);
                    app_ui_time_set_date(current_time.year, current_time.month, current_time.day);
                    app_ui_time_set_main_table_time(current_time.hour, current_time.minute);
                    example_lvgl_unlock();
                }
            }
        }
    }
}

static void task_weather_update(void* pvParameters) {
    while (1) {
        if (xSemaphoreTake(weather_Sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "收到天气获取请求");

            if (!g_wifi_connected) {
                ESP_LOGW(TAG, "天气获取失败：未连接WiFi");
                continue;
            }

            vTaskDelay(pdMS_TO_TICKS(1000));

            ESP_LOGI(TAG, "开始获取天气");
            app_weather_init();
            app_weather_get("101280101", weather_result_callback);
        }
    }
}


void app_controller_init(void)
{
    lux_value_queue = xQueueCreate(1, sizeof(uint32_t));
    wifi_operation_queue = xQueueCreate(10, sizeof(WifiOperationMsg));
    location_Sem = xSemaphoreCreateBinary();
    time_sync_Sem = xSemaphoreCreateBinary();
    weather_Sem = xSemaphoreCreateBinary();

    hw_wifi_register_state_cb(wifi_state_callback);

    xTaskCreate(task_get_lux_value, "task_get_lux_value", 2048, NULL, 5, NULL);
    xTaskCreate(task_calc_exposure, "task_calc_exposure", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(task_wifi_operation, "task_wifi_operation", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_get_location, "task_get_location", 4096, NULL, 5, NULL, 1);
    xTaskCreate(task_time_sync_and_update, "task_time_sync", 4096, NULL, 5, NULL);
    xTaskCreate(task_time_update_periodic, "task_time_update", 2048, NULL, 5, NULL);
    xTaskCreate(task_weather_update, "task_weather", 16384, NULL, 5, NULL);
}
