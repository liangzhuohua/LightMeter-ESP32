#include "app_controller.h"
#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "app_ui_wifi_port.h"
#include "app_ui_location_port.h"
#include "app_ui_time_port.h"
#include "app_ui_weather_port.h"
#include "app_exposure_calc.h"
#include "app_time.h"
#include "app_weather.h"
#include "app_nvs_storage.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "hw_veml7700.h"
#include "hw_oled.h"
#include "hw_wifi.h"
#include "hw_max17055.h"
#include "app_ui_battery_port.h"
#include "app_location.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_sleep.h"

static const char* TAG = "app_controller";

#define MAX_RETRIES 3

#define TIME_SYNC_THRESHOLD_US    (6LL * 60 * 60 * 1000000)         // 6小时
#define WEATHER_SYNC_THRESHOLD_US (30LL * 60 * 1000000)              // 30分钟
#define LOCATION_SYNC_THRESHOLD_US (24LL * 60 * 60 * 1000000)       // 24小时

#define TIME_SYNC_INTERVAL_MS     (30 * 60 * 1000)                   // 30分钟
#define WEATHER_SYNC_INTERVAL_MS  (30 * 60 * 1000)                  // 30分钟

typedef struct {
    bool location_done;
    bool time_done;
    bool weather_done;
    bool location_success;
    bool time_success;
    bool weather_success;
    bool location_need_retry;
    bool time_need_retry;
    int location_retries;
    int time_retries;
    int weather_retries;
} request_status_t;

static request_status_t g_req_status = {0};

static void log_memory(const char* stage) {
    ESP_LOGI(TAG, "[MEM-%s] Free: %lu, Largest: %lu",
             stage,
             (unsigned long)esp_get_free_heap_size(),
             (unsigned long)heap_caps_get_largest_free_block(MALLOC_CAP_DEFAULT));
}

static void reset_request_status(void) {
    memset(&g_req_status, 0, sizeof(request_status_t));
}

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
static bool g_location_ready = false;
static double g_latitude = 0.0;
static double g_longitude = 0.0;
static char g_auto_connect_ssid[33] = {0};
static char g_auto_connect_password[65] = {0};
static bool g_auto_connect_pending = false;
static int g_auto_connect_retries = 0;
static bool g_periodic_sync_mode = false;

static void weather_result_callback(const weather_data_t* data);

static void wifi_state_callback(const hw_wifi_state_event_t *event) {
    if (event == NULL) {
        return;
    }

    switch (event->state) {
        case HW_WIFI_STATE_CONNECTING:
            if (example_lvgl_lock(-1)) {
                app_ui_wifi_on_connecting(event->ssid);
                example_lvgl_unlock();
            }
            g_wifi_connected = false;
            break;

        case HW_WIFI_STATE_CONNECTED:
            if (example_lvgl_lock(-1)) {
                app_ui_wifi_on_connected(event->ssid);
                example_lvgl_unlock();
            }
            g_wifi_connected = true;
            g_auto_connect_pending = false;
            g_auto_connect_retries = 0;
            app_nvs_save_all();
            break;

        case HW_WIFI_STATE_DISCONNECTED:
            if (example_lvgl_lock(-1)) {
                app_ui_wifi_on_disconnected(event->ssid);
                example_lvgl_unlock();
            }
            g_wifi_connected = false;
            break;

        case HW_WIFI_STATE_CONNECT_FAILED:
            if (example_lvgl_lock(-1)) {
                app_ui_wifi_on_connect_failed(event->ssid, event->reason);
                example_lvgl_unlock();
            }
            g_wifi_connected = false;

            if (g_auto_connect_pending && g_auto_connect_retries < 3) {
                g_auto_connect_retries++;
                ESP_LOGI(TAG, "自动连接失败，重新扫描重试 (%d/3)", g_auto_connect_retries);
                vTaskDelay(pdMS_TO_TICKS(1000));
                hw_wifi_scan_async();
            } else if (g_auto_connect_pending) {
                ESP_LOGW(TAG, "自动连接已达到最大重试次数");
                g_auto_connect_pending = false;
                g_auto_connect_retries = 0;
            }
            break;

        default:
            break;
    }

    if (event->state == HW_WIFI_STATE_CONNECTED && g_wifi_scanned) {
        ESP_LOGI(TAG, "WiFi已连接，开始串行请求：1.定位 -> 2.时间 -> 3.天气");
        reset_request_status();
        log_memory("WiFi连接后");
        xSemaphoreGive(location_Sem);
    }
}

static void wifi_scan_done_callback(wifi_scan_result_t* result) {
    if (result == NULL || result->count == 0 || result->ap_list == NULL) {
        ESP_LOGW(TAG, "WiFi扫描结果为空");
        g_wifi_scanned = false;

        if (g_auto_connect_pending) {
            ESP_LOGI(TAG, "扫描结果为空，无法自动连接");
            g_auto_connect_pending = false;
        }
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

    // 检查是否需要自动连接
    if (g_auto_connect_pending && g_auto_connect_ssid[0] != '\0') {
        bool found = false;
        for (int i = 0; i < result->count; i++) {
            if (strcmp(result->ap_list[i].ssid, g_auto_connect_ssid) == 0) {
                found = true;
                ESP_LOGI(TAG, "找到保存的WiFi: %s，开始连接", g_auto_connect_ssid);
                break;
            }
        }

        if (found) {
            hw_wifi_connect(g_auto_connect_ssid, g_auto_connect_password);
        } else {
            ESP_LOGI(TAG, "未找到保存的WiFi: %s，跳过连接", g_auto_connect_ssid);
            g_auto_connect_pending = false;
            g_auto_connect_retries = 0;
        }
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
            ui_calc_port_set_shutter_to_roller(main_roller_shutter, calc_data.shutter, cam);
            ui_calc_port_set_aperture_to_roller(main_roller_aperture, calc_data.aperture, len);
            ui_calc_port_update_roller_warning_color(main_roller_shutter, main_roller_aperture, calc_data.flags);
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
                app_nvs_set_wifi_enabled(true);
                break;

            case WIFI_OP_DISABLE:
                hw_wifi_deinit();
                app_nvs_set_wifi_enabled(false);
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
    log_memory("定位后");

    if (result == NULL) {
        ESP_LOGE(TAG, "定位结果为空");
        g_req_status.location_retries++;

        if (g_req_status.location_retries < MAX_RETRIES) {
            ESP_LOGI(TAG, "定位失败，准备异步重试 %d/%d", g_req_status.location_retries + 1, MAX_RETRIES);
            g_req_status.location_need_retry = true;
            return;
        }

        ESP_LOGE(TAG, "定位失败，已达到最大重试次数");
        if (example_lvgl_lock(-1)) {
            app_ui_location_set_unknown();
            example_lvgl_unlock();
        }
        g_location_ready = false;
        g_req_status.location_success = false;
    } else {
        ESP_LOGI(TAG, "定位结果回调:");
        ESP_LOGI(TAG, "  纬度: %f", result->latitude);
        ESP_LOGI(TAG, "  经度: %f", result->longitude);
        ESP_LOGI(TAG, "  精度: %f 米", result->accuracy);
        ESP_LOGI(TAG, "  地址: %s", result->address);

        g_latitude = result->latitude;
        g_longitude = result->longitude;
        g_location_ready = true;
        g_req_status.location_success = true;
        app_nvs_update_location_sync_timestamp();

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
            if (example_lvgl_lock(-1)) {
                app_ui_location_set_city(city);
                example_lvgl_unlock();
            }
        } else {
            if (example_lvgl_lock(-1)) {
                app_ui_location_set_city("Unknown");
                example_lvgl_unlock();
            }
            strncpy(city, "Unknown", sizeof(city) - 1);
        }

        if (detail[0] != '\0') {
            if (example_lvgl_lock(-1)) {
                app_ui_location_set_detail(detail);
                example_lvgl_unlock();
            }
        } else {
            if (example_lvgl_lock(-1)) {
                app_ui_location_set_detail(result->address);
                example_lvgl_unlock();
            }
            strncpy(detail, result->address, sizeof(detail) - 1);
            detail[sizeof(detail) - 1] = '\0';
        }

        app_nvs_set_location(result->latitude, result->longitude, true);
        app_nvs_set_location_text(city, detail);
        app_nvs_save_location();
    }

    ESP_LOGI(TAG, "定位完成(%s)，触发时间同步", g_req_status.location_success ? "成功" : "失败");
    g_req_status.location_done = true;
    app_time_sntp_init();
    xSemaphoreGive(time_sync_Sem);
}

static void weather_result_callback(const weather_data_t* data) {
    if (data == NULL) {
        ESP_LOGE(TAG, "获取天气失败");
        g_req_status.weather_retries++;

        if (g_req_status.weather_retries < MAX_RETRIES) {
            ESP_LOGI(TAG, "天气获取失败，准备异步重试 %d/%d", g_req_status.weather_retries + 1, MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(1000));
            xSemaphoreGive(weather_Sem);
            return;
        }

        ESP_LOGE(TAG, "天气获取失败，已达到最大重试次数");
        g_req_status.weather_success = false;
    } else {
        ESP_LOGI(TAG, "获取天气成功: %s, 温度: %d, 湿度: %d", data->desc, data->temp, data->humidity);

        if (example_lvgl_lock(-1)) {
            app_ui_weather_update_all(data);
            example_lvgl_unlock();
        }

        app_nvs_set_weather(data);
        app_nvs_update_weather_sync_timestamp();
        app_nvs_save_weather();

        g_req_status.weather_success = true;
    }

    g_req_status.weather_done = true;

    ESP_LOGI(TAG, "全部同步流程结束，最终状态:");
    ESP_LOGI(TAG, "  定位: %s", g_req_status.location_success ? "成功" : "失败");
    ESP_LOGI(TAG, "  时间: %s", g_req_status.time_success ? "成功" : "失败");
    ESP_LOGI(TAG, "  天气: %s", g_req_status.weather_success ? "成功" : "失败");
    ESP_LOGI(TAG, "==================================");

    g_periodic_sync_mode = false;
}

static void task_get_location(void* pvParameters) {
    while (1) {
        if (xSemaphoreTake(location_Sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "收到定位请求");
            log_memory("定位前");

            if (!g_wifi_scanned) {
                ESP_LOGW(TAG, "定位失败：未扫描WiFi，跳过继续时间同步");
                g_req_status.location_done = true;
                g_req_status.location_success = false;
                app_time_sntp_init();
                xSemaphoreGive(time_sync_Sem);
                continue;
            }

            if (!g_wifi_connected) {
                ESP_LOGW(TAG, "定位失败：未连接WiFi，跳过继续时间同步");
                g_req_status.location_done = true;
                g_req_status.location_success = false;
                app_time_sntp_init();
                xSemaphoreGive(time_sync_Sem);
                continue;
            }

            if (g_wifi_scan_result.count == 0 || g_wifi_scan_result.ap_list == NULL) {
                ESP_LOGW(TAG, "定位失败：扫描结果为空，跳过继续时间同步");
                g_req_status.location_done = true;
                g_req_status.location_success = false;
                app_time_sntp_init();
                xSemaphoreGive(time_sync_Sem);
                continue;
            }

            ESP_LOGI(TAG, "开始定位，WiFi数量: %d (重试 %d/%d)",
                     g_wifi_scan_result.count, g_req_status.location_retries + 1, MAX_RETRIES);

            g_req_status.location_need_retry = false;
            app_location_get_location(&g_wifi_scan_result, location_result_callback);

            if (g_req_status.location_need_retry) {
                ESP_LOGI(TAG, "异步重试定位...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                xSemaphoreGive(location_Sem);
                continue;
            }
        }
    }
}

static void task_time_sync_and_update(void* pvParameters) {
    app_time_t current_time;

    while (1) {
        if (xSemaphoreTake(time_sync_Sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "收到时间同步请求");
            log_memory("时间同步前");

            if (!g_wifi_connected) {
                ESP_LOGW(TAG, "时间同步失败：未连接WiFi，跳过继续天气请求");
                g_req_status.time_done = true;
                g_req_status.time_success = false;
                xSemaphoreGive(weather_Sem);
                continue;
            }

            if (!app_time_is_synced()) {
                ESP_LOGI(TAG, "开始同步时间 (重试 %d/%d)",
                         g_req_status.time_retries + 1, MAX_RETRIES);
                app_time_sntp_sync();
                app_time_wait_sync(10000);
            }

            log_memory("时间同步后");

            if (app_time_is_synced()) {
                g_time_synced = true;
                g_req_status.time_success = true;
                app_nvs_update_time_sync_timestamp();
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
                g_req_status.time_retries++;

                if (g_req_status.time_retries < MAX_RETRIES) {
                    ESP_LOGW(TAG, "时间同步失败，准备重试 %d/%d",
                             g_req_status.time_retries + 1, MAX_RETRIES);
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    xSemaphoreGive(time_sync_Sem);
                    continue;
                }

                ESP_LOGW(TAG, "时间同步失败，已达到最大重试次数");
            }

            g_req_status.time_done = true;

            if (g_periodic_sync_mode) {
                ESP_LOGI(TAG, "时间同步完成(%s)，定时同步模式不触发天气", g_req_status.time_success ? "成功" : "失败");
                g_periodic_sync_mode = false;
            } else {
                ESP_LOGI(TAG, "时间同步完成(%s)，触发天气请求", g_req_status.time_success ? "成功" : "失败");
                xSemaphoreGive(weather_Sem);
            }
        }
    }
}

static void periodic_sync_timer_cb(lv_timer_t* timer) {
    if (!g_wifi_connected) {
        ESP_LOGD(TAG, "WiFi未连接，跳过定时同步");
        return;
    }

    ESP_LOGI(TAG, "定时同步检查...");

    sync_timestamp_t ts;
    app_nvs_load_sync_timestamps(&ts);

    int64_t now = esp_timer_get_time();

    if (now - ts.last_time_sync > TIME_SYNC_THRESHOLD_US) {
        ESP_LOGI(TAG, "时间超过阈值，触发同步");
        reset_request_status();
        g_periodic_sync_mode = true;
        xSemaphoreGive(time_sync_Sem);
        return;
    }

    if (now - ts.last_weather_sync > WEATHER_SYNC_THRESHOLD_US) {
        ESP_LOGI(TAG, "天气超过阈值，触发同步");
        reset_request_status();
        g_periodic_sync_mode = true;
        xSemaphoreGive(weather_Sem);
        return;
    }
}

static void check_sync_on_startup(void) {
    sync_timestamp_t ts;
    if (app_nvs_load_sync_timestamps(&ts) != 0) {
        ESP_LOGI(TAG, "无同步时间戳记录，将进行首次同步");
        return;
    }

    int64_t now = esp_timer_get_time();

    ESP_LOGI(TAG, "同步时间戳检查:");
    ESP_LOGI(TAG, "  时间同步: %lld us 前", (now - ts.last_time_sync) / 1000000);
    ESP_LOGI(TAG, "  天气同步: %lld us 前", (now - ts.last_weather_sync) / 1000000);
    ESP_LOGI(TAG, "  位置同步: %lld us 前", (now - ts.last_location_sync) / 1000000);

    if (now - ts.last_time_sync > TIME_SYNC_THRESHOLD_US) {
        ESP_LOGI(TAG, "时间同步已过期，需要在WiFi连接后同步");
    }

    if (now - ts.last_weather_sync > WEATHER_SYNC_THRESHOLD_US) {
        ESP_LOGI(TAG, "天气同步已过期，需要在WiFi连接后同步");
    }
}

static void task_time_update_periodic(void* pvParameters) {
    app_time_t current_time;
    int last_minute = -1;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));

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

static void task_battery_update(void* pvParameters) {
    float soc = 0.0f;
    float voltage = 0.0f;

    while (1) {
        hw_max17055_get_soc(&soc);
        hw_max17055_get_vcell(&voltage);

        if (example_lvgl_lock(-1)) {
            app_ui_battery_update(soc, voltage);
            example_lvgl_unlock();
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

static void task_weather_update(void* pvParameters) {
    while (1) {
        if (xSemaphoreTake(weather_Sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "收到天气获取请求");
            log_memory("天气请求前");

            if (!g_wifi_connected) {
                ESP_LOGW(TAG, "天气获取失败：未连接WiFi");
                g_req_status.weather_done = true;
                g_req_status.weather_success = false;
                ESP_LOGI(TAG, "========== 请求完成 ==========");
                ESP_LOGI(TAG, "定位: %s", g_req_status.location_success ? "成功" : "失败");
                ESP_LOGI(TAG, "时间: %s", g_req_status.time_success ? "成功" : "失败");
                ESP_LOGI(TAG, "天气: %s", g_req_status.weather_success ? "成功" : "失败");
                ESP_LOGI(TAG, "==============================");
                continue;
            }

            if (!g_location_ready) {
                ESP_LOGW(TAG, "定位未成功，使用默认位置（广州）");
                g_latitude = 23.12;
                g_longitude = 113.26;
            }

            vTaskDelay(pdMS_TO_TICKS(500));

            char location_str[32];
            snprintf(location_str, sizeof(location_str), "%.2f,%.2f", g_longitude, g_latitude);

            ESP_LOGI(TAG, "开始获取天气，位置: %s (重试 %d/%d)",
                     location_str, g_req_status.weather_retries + 1, MAX_RETRIES);
            app_weather_init();
            app_weather_get(location_str, weather_result_callback);
        }
    }
}


static SemaphoreHandle_t sleep_sem = NULL;

static void task_power_manage(void* arg) {
    while (1) {
        if (xSemaphoreTake(sleep_sem, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Wakeup key long pressed, entering deep sleep...");
            app_controller_enter_deep_sleep();
        }
    }
}

static void wakeup_key_callback(wakeup_key_event_t event) {
    if (event == WAKEUP_KEY_EVENT_LONG_PRESS) {
        if (sleep_sem != NULL) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xSemaphoreGiveFromISR(sleep_sem, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken) {
                portYIELD_FROM_ISR();
            }
        }
    }
}

void app_controller_wakeup_key_init(void) {
    if (sleep_sem == NULL) {
        sleep_sem = xSemaphoreCreateBinary();
        xTaskCreate(task_power_manage, "task_power_manage", 4096, NULL, 5, NULL);
    }

    hw_wakeup_key_init();
    if (hw_wakeup_key_check_wakeup()) {
        ESP_LOGI(TAG, "Woke up from deep sleep by wakeup key");
        // Additional logic if needed on wakeup
    }
    hw_wakeup_key_set_callback(wakeup_key_callback);
}

void app_controller_enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Preparing to enter deep sleep...");

    app_time_save_to_rtc();

    oled_set_brightness(0);

    if (g_wifi_connected) {
        WifiOperationMsg msg = { .op = WIFI_OP_DISCONNECT };
        xQueueSend(wifi_operation_queue, &msg, 0);
    }

    hw_wakeup_key_enable_sleep_wakeup();

    ESP_LOGI(TAG, "Entering deep sleep now");
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_deep_sleep_start();
}

void app_controller_init(void)
{
    lux_value_queue = xQueueCreate(1, sizeof(uint32_t));
    wifi_operation_queue = xQueueCreate(10, sizeof(WifiOperationMsg));
    location_Sem = xSemaphoreCreateBinary();
    time_sync_Sem = xSemaphoreCreateBinary();
    weather_Sem = xSemaphoreCreateBinary();

    hw_wifi_register_state_cb(wifi_state_callback);

    ESP_LOGI(TAG, "从NVS加载数据...");
    app_nvs_load_all();

    app_controller_wakeup_key_init();

    if (app_time_restore_from_rtc() == ESP_OK) {
        ESP_LOGI(TAG, "从RTC内存恢复时间成功");
    } else {
        ESP_LOGW(TAG, "RTC内存无有效时间备份");
    }

    app_time_t current_time;
    if (app_time_get_now(&current_time) == ESP_OK) {
        ESP_LOGI(TAG, "唤醒时读取本地时间: %04d-%02d-%02d %02d:%02d:%02d",
                 current_time.year, current_time.month, current_time.day,
                 current_time.hour, current_time.minute, current_time.second);
        if (example_lvgl_lock(-1)) {
            app_ui_time_set_time(current_time.hour, current_time.minute);
            app_ui_time_set_date(current_time.year, current_time.month, current_time.day);
            app_ui_time_set_main_table_time(current_time.hour, current_time.minute);
            example_lvgl_unlock();
        }
    }

    location_data_t cached_loc;
    weather_data_t cached_weather;

    bool has_location = (app_nvs_get_location_data(&cached_loc) == 0 && cached_loc.valid);
    bool has_weather = (app_nvs_get_weather_data(&cached_weather) == 0);

    if (has_location) {
        g_latitude = cached_loc.latitude;
        g_longitude = cached_loc.longitude;
        g_location_ready = cached_loc.valid;
        ESP_LOGI(TAG, "恢复缓存位置: %.6f, %.6f, city=%s", g_latitude, g_longitude, cached_loc.city);

        if (cached_loc.city[0] != '\0') {
            if (example_lvgl_lock(-1)) {
                app_ui_location_set_city(cached_loc.city);
                example_lvgl_unlock();
            }
        }
        if (cached_loc.detail[0] != '\0') {
            if (example_lvgl_lock(-1)) {
                app_ui_location_set_detail(cached_loc.detail);
                example_lvgl_unlock();
            }
        }
    }

    wifi_data_t wifi_config;
    bool has_wifi_config = (app_nvs_get_wifi_config(&wifi_config) == 0 && wifi_config.ssid[0] != '\0');

    if (has_wifi_config) {
        if (example_lvgl_lock(-1)) {
            app_ui_wifi_set_enabled(wifi_config.enabled);
            example_lvgl_unlock();
        }

        if (wifi_config.enabled) {
            ESP_LOGI(TAG, "WiFi已启用，扫描并尝试连接: %s", wifi_config.ssid);

            strncpy(g_auto_connect_ssid, wifi_config.ssid, sizeof(g_auto_connect_ssid) - 1);
            strncpy(g_auto_connect_password, wifi_config.password, sizeof(g_auto_connect_password) - 1);
            g_auto_connect_pending = true;

            hw_wifi_init_with_scan_cb(wifi_scan_done_callback);
            vTaskDelay(pdMS_TO_TICKS(500));
            hw_wifi_scan_async();
        } else {
            ESP_LOGI(TAG, "WiFi已禁用，跳过自动连接");
        }
    } else {
        ESP_LOGI(TAG, "无WiFi配置");
        if (example_lvgl_lock(-1)) {
            app_ui_wifi_set_enabled(false);
            example_lvgl_unlock();
        }
    }

    if (has_weather) {
        ESP_LOGI(TAG, "恢复缓存天气: %d°C, %s", cached_weather.temp, cached_weather.desc);
        if (example_lvgl_lock(-1)) {
            app_ui_weather_update_all(&cached_weather);
            example_lvgl_unlock();
        }
    }

    check_sync_on_startup();

    lv_timer_t* sync_timer = lv_timer_create(periodic_sync_timer_cb, WEATHER_SYNC_INTERVAL_MS, NULL);
    if (sync_timer == NULL) {
        ESP_LOGW(TAG, "创建定时同步定时器失败");
    } else {
        ESP_LOGI(TAG, "定时同步定时器已创建，间隔: %d ms", WEATHER_SYNC_INTERVAL_MS);
    }

    xTaskCreate(task_get_lux_value, "task_get_lux_value", 4096, NULL, 5, NULL);
    xTaskCreate(task_calc_exposure, "task_calc_exposure", 4096, NULL, 5, NULL);
    xTaskCreatePinnedToCore(task_wifi_operation, "task_wifi_operation", 4096, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_get_location, "task_get_location", 8192, NULL, 5, NULL, 0);
    xTaskCreatePinnedToCore(task_time_sync_and_update, "task_time_sync", 6144, NULL, 5, NULL, 0);
    xTaskCreate(task_time_update_periodic, "task_time_update", 2048, NULL, 5, NULL);
    xTaskCreatePinnedToCore(task_weather_update, "task_weather", 16384, NULL, 5, NULL, 0);

    hw_max17055_init();
    xTaskCreate(task_battery_update, "task_battery", 3072, NULL, 5, NULL);
}

const char* app_controller_get_current_ssid(void) {
    return hw_wifi_get_current_ssid();
}

double app_controller_get_latitude(void) {
    return g_latitude;
}

double app_controller_get_longitude(void) {
    return g_longitude;
}

bool app_controller_get_location_valid(void) {
    return g_location_ready;
}
