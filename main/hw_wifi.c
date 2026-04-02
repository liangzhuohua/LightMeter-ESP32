#include "hw_wifi.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>

static const char* TAG = "hw_wifi";

static uint8_t WIFI_AUTO_RECONNECT_ENABLED = 0;
static uint8_t MAX_AP_SCAN_NUM = 10;
static uint8_t WIFI_AUTO_RECONNECT_RETRIES_NOW = 0;
static uint8_t WIFI_AUTO_RECONNECT_MAX_RETRIES = 5;

static wifi_scan_done_cb_t g_scan_done_callback = NULL;
static wifi_state_cb_t g_wifi_state_callback = NULL;
static char g_current_ssid[33] = {0};
static char g_pending_ssid[33] = {0};
static char g_pending_password[65] = {0};
static uint8_t g_is_connected = 0;
static uint8_t g_switch_pending = 0;
static uint8_t g_manual_disconnect = 0;
static uint8_t g_connect_request_active = 0;

static void notify_wifi_state(hw_wifi_state_t state, const char *ssid, int reason) {
    if (g_wifi_state_callback == NULL) {
        return;
    }

    hw_wifi_state_event_t event = {0};
    event.state = state;
    event.reason = reason;
    if (ssid != NULL) {
        strncpy(event.ssid, ssid, sizeof(event.ssid) - 1);
        event.ssid[sizeof(event.ssid) - 1] = '\0';
    }

    g_wifi_state_callback(&event);
}

static void start_connect_with_pending_config(void) {
    wifi_config_t wifi_config = {
        .sta = {}
    };

    strncpy((char *)wifi_config.sta.ssid, g_pending_ssid, sizeof(wifi_config.sta.ssid) - 1);
    wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    strncpy((char *)wifi_config.sta.password, g_pending_password, sizeof(wifi_config.sta.password) - 1);
    wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());
}


static void print_auth_mode(wifi_auth_mode_t authmode) {
    switch (authmode) {
        case WIFI_AUTH_OPEN: ESP_LOGI("SCAN", "Auth Mode \tOpen"); break;
        case WIFI_AUTH_WEP: ESP_LOGI("SCAN", "Auth Mode \tWEP"); break;
        case WIFI_AUTH_WPA_PSK: ESP_LOGI("SCAN", "Auth Mode \tWPA-PSK"); break;
        case WIFI_AUTH_WPA2_PSK: ESP_LOGI("SCAN", "Auth Mode \tWPA2-PSK"); break;
        case WIFI_AUTH_WPA3_PSK: ESP_LOGI("SCAN", "Auth Mode \tWPA3-PSK"); break;
        default: ESP_LOGI("SCAN", "Auth Mode \tUnknown"); break;
    }
}

static void print_cipher_type(wifi_cipher_type_t pairwise, wifi_cipher_type_t group) {
    ESP_LOGI("SCAN", "Pairwise Cipher \t%s",
             (pairwise == WIFI_CIPHER_TYPE_NONE) ? "None" :
             (pairwise == WIFI_CIPHER_TYPE_TKIP) ? "TKIP" :
             (pairwise == WIFI_CIPHER_TYPE_CCMP) ? "CCMP" : "Unknown");

    ESP_LOGI("SCAN", "Group Cipher \t\t%s",
             (group == WIFI_CIPHER_TYPE_NONE) ? "None" :
             (group == WIFI_CIPHER_TYPE_TKIP) ? "TKIP" :
             (group == WIFI_CIPHER_TYPE_CCMP) ? "CCMP" : "Unknown");
}

static void wifi_scan_done_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    wifi_event_sta_scan_done_t* scan_data = (wifi_event_sta_scan_done_t*)event_data;

    if (scan_data->status != 0) {
        ESP_LOGE("SCAN", "扫描失败，状态: %d", scan_data->status);
        if (g_scan_done_callback) {
            wifi_scan_result_t empty_result = {0};
            g_scan_done_callback(&empty_result);
        }
        return;
    }

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    uint16_t max_ap = (ap_count > MAX_AP_SCAN_NUM) ? MAX_AP_SCAN_NUM : ap_count;
    wifi_ap_record_t* ap_records = malloc(sizeof(wifi_ap_record_t) * max_ap);
    if (!ap_records) {
        ESP_LOGE("SCAN", "内存分配失败");
        if (g_scan_done_callback) {
            wifi_scan_result_t empty_result = {0};
            g_scan_done_callback(&empty_result);
        }
        return;
    }

    uint16_t ap_num = max_ap;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    wifi_scan_result_t result = {0};
    result.ap_list = malloc(sizeof(wifi_info_t) * ap_num);
    if (!result.ap_list) {
        ESP_LOGE("SCAN", "内存分配失败");
        free(ap_records);
        if (g_scan_done_callback) {
            wifi_scan_result_t empty_result = {0};
            g_scan_done_callback(&empty_result);
        }
        return;
    }

    result.count = ap_num;

    ESP_LOGI("SCAN", "发现 %d 个WiFi热点:", ap_num);
    for (int i = 0; i < ap_num; i++) {
        strncpy(result.ap_list[i].ssid, (char*)ap_records[i].ssid, 32);
        result.ap_list[i].ssid[32] = '\0';
        result.ap_list[i].rssi = ap_records[i].rssi;
        result.ap_list[i].channel = ap_records[i].primary;
        result.ap_list[i].authmode = ap_records[i].authmode;
        memcpy(result.ap_list[i].bssid, ap_records[i].bssid, 6);

        ESP_LOGI("SCAN", "--- AP %d ---", i + 1);
        ESP_LOGI("SCAN", "SSID: \t\t%s", result.ap_list[i].ssid);
        ESP_LOGI("SCAN", "RSSI: \t\t%d dBm", result.ap_list[i].rssi);
        ESP_LOGI("SCAN", "Channel: \t%d", result.ap_list[i].channel);
        ESP_LOGI("SCAN", "BSSID: \t\t%02X:%02X:%02X:%02X:%02X:%02X",
                 result.ap_list[i].bssid[0], result.ap_list[i].bssid[1],
                 result.ap_list[i].bssid[2], result.ap_list[i].bssid[3],
                 result.ap_list[i].bssid[4], result.ap_list[i].bssid[5]);
    }

    free(ap_records);

    if (g_scan_done_callback) {
        g_scan_done_callback(&result);
    }
}

/**
 * WIFI事件处理函数（STA模式）
 * 处理STA模式下的WIFI事件
 * @param arg 事件处理函数的上下文
 * @param event_base 事件基类
 * @param event_id 事件ID
 * @param event_data 事件数据
 */
static void wifi_event_handler_STA(void *arg, esp_event_base_t event_base,int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi驱动就绪");
                break;

            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "已连接到热点");
                break;

            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
                char disconnected_ssid[33] = {0};

                if (g_current_ssid[0] != '\0') {
                    strncpy(disconnected_ssid, g_current_ssid, sizeof(disconnected_ssid) - 1);
                } else if (g_pending_ssid[0] != '\0') {
                    strncpy(disconnected_ssid, g_pending_ssid, sizeof(disconnected_ssid) - 1);
                }

                ESP_LOGW(TAG, "热点断开, reason=%d", event ? event->reason : -1);

                if (g_is_connected && disconnected_ssid[0] != '\0') {
                    notify_wifi_state(HW_WIFI_STATE_DISCONNECTED, disconnected_ssid, event ? event->reason : 0);
                }

                g_is_connected = 0;
                g_current_ssid[0] = '\0';

                if (g_switch_pending) {
                    g_switch_pending = 0;
                    start_connect_with_pending_config();
                    return;
                }

                if (g_manual_disconnect) {
                    g_manual_disconnect = 0;
                    g_connect_request_active = 0;
                    g_pending_ssid[0] = '\0';
                    g_pending_password[0] = '\0';
                    return;
                }

                if (g_connect_request_active) {
                    notify_wifi_state(HW_WIFI_STATE_CONNECT_FAILED, g_pending_ssid, event ? event->reason : 0);
                    g_connect_request_active = 0;
                    g_pending_ssid[0] = '\0';
                    g_pending_password[0] = '\0';
                }
                break;
            }

            default:
                break;
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "成功获取IP:" IPSTR, IP2STR(&event->ip_info.ip));

        WIFI_AUTO_RECONNECT_RETRIES_NOW = 0;
        WIFI_AUTO_RECONNECT_ENABLED = 0;
        g_is_connected = 1;
        g_connect_request_active = 0;

        if (g_pending_ssid[0] != '\0') {
            strncpy(g_current_ssid, g_pending_ssid, sizeof(g_current_ssid) - 1);
            g_current_ssid[sizeof(g_current_ssid) - 1] = '\0';
        }

        notify_wifi_state(HW_WIFI_STATE_CONNECTED, g_current_ssid, 0);
    }
}

void hw_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "wifi_init_success");

    esp_event_handler_instance_t wifi_event_handle, ip_event_handle;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,&wifi_event_handler_STA, NULL, &wifi_event_handle);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,&wifi_event_handler_STA, NULL, &ip_event_handle);
    ESP_LOGI(TAG, "wifi_init_success");
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}

void hw_wifi_init_with_scan_cb(wifi_scan_done_cb_t callback) {
    g_scan_done_callback = callback;

    ESP_ERROR_CHECK(esp_netif_init());
    esp_event_loop_create_default();
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_LOGI(TAG, "wifi_init_success");

    esp_event_handler_instance_t wifi_event_handle, ip_event_handle, scan_done_handle;
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler_STA, NULL, &wifi_event_handle);
    esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler_STA, NULL, &ip_event_handle);
    esp_event_handler_instance_register(WIFI_EVENT, WIFI_EVENT_SCAN_DONE, &wifi_scan_done_handler, NULL, &scan_done_handle);

    ESP_LOGI(TAG, "wifi_init_success with scan callback");
    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_start();
}

void hw_wifi_register_state_cb(wifi_state_cb_t callback) {
    g_wifi_state_callback = callback;
}

void hw_wifi_scan_async(void) {
    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, false));
    ESP_LOGI("SCAN", "开始异步扫描...");
}

wifi_scan_result_t hw_wifi_scan(void) {
    wifi_scan_result_t result = {0};

    ESP_ERROR_CHECK(esp_wifi_scan_start(NULL, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));

    uint16_t max_ap = (ap_count > MAX_AP_SCAN_NUM) ? MAX_AP_SCAN_NUM : ap_count;
    wifi_ap_record_t *ap_records = malloc(sizeof(wifi_ap_record_t) * max_ap);
    if (!ap_records) {
        ESP_LOGE("SCAN", "内存分配失败");
        return result;
    }

    uint16_t ap_num = max_ap;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_num, ap_records));

    result.ap_list = malloc(sizeof(wifi_info_t) * ap_num);
    if (!result.ap_list) {
        ESP_LOGE("SCAN", "内存分配失败");
        free(ap_records);
        return result;
    }

    result.count = ap_num;

    ESP_LOGI("SCAN", "发现 %d 个WiFi热点:", ap_num);
    for (int i = 0; i < ap_num; i++) {
        strncpy(result.ap_list[i].ssid, (char*)ap_records[i].ssid, 32);
        result.ap_list[i].ssid[32] = '\0';
        result.ap_list[i].rssi = ap_records[i].rssi;
        result.ap_list[i].channel = ap_records[i].primary;
        result.ap_list[i].authmode = ap_records[i].authmode;
        memcpy(result.ap_list[i].bssid, ap_records[i].bssid, 6);

        ESP_LOGI("SCAN", "--- AP %d ---", i + 1);
        ESP_LOGI("SCAN", "SSID: \t\t%s", result.ap_list[i].ssid);
        ESP_LOGI("SCAN", "RSSI: \t\t%d dBm", result.ap_list[i].rssi);
        ESP_LOGI("SCAN", "Channel: \t%d", result.ap_list[i].channel);
        ESP_LOGI("SCAN", "BSSID: \t\t%02X:%02X:%02X:%02X:%02X:%02X",
                 result.ap_list[i].bssid[0], result.ap_list[i].bssid[1],
                 result.ap_list[i].bssid[2], result.ap_list[i].bssid[3],
                 result.ap_list[i].bssid[4], result.ap_list[i].bssid[5]);
    }

    free(ap_records);
    return result;
}

void hw_wifi_scan_result_free(wifi_scan_result_t* result) {
    if (result && result->ap_list) {
        free(result->ap_list);
        result->ap_list = NULL;
        result->count = 0;
    }
}

void hw_wifi_deinit(void) {
    if (g_is_connected && g_current_ssid[0] != '\0') {
        notify_wifi_state(HW_WIFI_STATE_DISCONNECTED, g_current_ssid, 0);
    }

    g_is_connected = 0;
    g_switch_pending = 0;
    g_manual_disconnect = 0;
    g_connect_request_active = 0;
    g_current_ssid[0] = '\0';
    g_pending_ssid[0] = '\0';
    g_pending_password[0] = '\0';

    esp_wifi_stop();
    esp_wifi_deinit();
    esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
    if (netif) {
        esp_netif_destroy(netif);
    }

    esp_event_loop_delete_default();
}


void hw_wifi_connect(char* SSID, char* PASSWORD) {
    if (SSID == NULL || SSID[0] == '\0') {
        return;
    }

    strncpy(g_pending_ssid, SSID, sizeof(g_pending_ssid) - 1);
    g_pending_ssid[sizeof(g_pending_ssid) - 1] = '\0';

    if (PASSWORD != NULL) {
        strncpy(g_pending_password, PASSWORD, sizeof(g_pending_password) - 1);
        g_pending_password[sizeof(g_pending_password) - 1] = '\0';
    } else {
        g_pending_password[0] = '\0';
    }

    g_manual_disconnect = 0;
    g_connect_request_active = 1;
    notify_wifi_state(HW_WIFI_STATE_CONNECTING, g_pending_ssid, 0);

    if (g_is_connected && strcmp(g_current_ssid, g_pending_ssid) != 0) {
        g_switch_pending = 1;
        ESP_ERROR_CHECK(esp_wifi_disconnect());
        return;
    }

    g_switch_pending = 0;
    start_connect_with_pending_config();
}

void hw_wifi_disconnect(void) {
    g_switch_pending = 0;
    g_connect_request_active = 0;
    g_pending_ssid[0] = '\0';
    g_pending_password[0] = '\0';
    g_manual_disconnect = 1;
    ESP_ERROR_CHECK(esp_wifi_disconnect());
}

void hw_wifi_auto_reconnect(void) {
    WIFI_AUTO_RECONNECT_ENABLED = 1;
    WIFI_AUTO_RECONNECT_RETRIES_NOW = 0;
    ESP_LOGI("WIFI", "自动重连已启用, 最大重试次数:%d", WIFI_AUTO_RECONNECT_MAX_RETRIES);
}

const char* hw_wifi_get_current_ssid(void) {
    return g_current_ssid;
}

const char* hw_wifi_get_current_password(void) {
    return g_pending_password;
}
