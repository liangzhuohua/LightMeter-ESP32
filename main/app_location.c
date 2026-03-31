#include "app_location.h"
#include <string.h>
#include <stdlib.h>
#include <esp_log.h>
#include <cJSON.h>
#include "app_http_requests.h"

static const char* TAG = "app_location";

#define LOCATION_URL "http://api.cellocation.com:84/loc/"

static location_result_callback_t g_location_callback = NULL;

static void http_response_handler(const char* response, int len, void* user_data) {
    if (response == NULL || len <= 0 || g_location_callback == NULL) {
        return;
    }

    ESP_LOGI(TAG, "解析定位响应...");

    cJSON *root = cJSON_ParseWithLength(response, len);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON解析失败");
        return;
    }

    location_result_t result = {0};

    cJSON *errcode = cJSON_GetObjectItem(root, "errcode");
    if (errcode && errcode->valueint == 0) {
        cJSON *lat = cJSON_GetObjectItem(root, "lat");
        cJSON *lon = cJSON_GetObjectItem(root, "lon");
        cJSON *radius = cJSON_GetObjectItem(root, "radius");
        cJSON *address = cJSON_GetObjectItem(root, "address");

        if (lat && cJSON_IsString(lat)) {
            result.latitude = atof(lat->valuestring);
        }
        if (lon && cJSON_IsString(lon)) {
            result.longitude = atof(lon->valuestring);
        }
        if (radius && cJSON_IsString(radius)) {
            result.accuracy = (float)atof(radius->valuestring);
        }
        if (address && cJSON_IsString(address)) {
            strncpy(result.address, address->valuestring, sizeof(result.address) - 1);
        }

        ESP_LOGI(TAG, "定位成功: lat=%f, lon=%f, accuracy=%f",
                 result.latitude, result.longitude, result.accuracy);
        ESP_LOGI(TAG, "地址: %s", result.address);

        g_location_callback(&result);
    } else {
        ESP_LOGE(TAG, "定位失败: errcode=%d", errcode ? errcode->valueint : -1);
    }

    cJSON_Delete(root);
}

void app_location_get_location(wifi_scan_result_t* result, location_result_callback_t callback) {
    if (result == NULL || result->count == 0 || result->ap_list == NULL) {
        ESP_LOGE(TAG, "WiFi扫描结果无效");
        return;
    }

    g_location_callback = callback;

    char *params = (char *)malloc(2048);
    if (params == NULL) {
        ESP_LOGE(TAG, "内存分配失败");
        return;
    }

    int offset = 0;
    offset += snprintf(params + offset, 2048 - offset, "wl=");

    for (int i = 0; i < result->count && offset < 2000; i++) {
        if (i > 0) {
            offset += snprintf(params + offset, 2048 - offset, ";");
        }
        offset += snprintf(params + offset, 2048 - offset, "%02x:%02x:%02x:%02x:%02x:%02x,%d",
                          result->ap_list[i].bssid[0],
                          result->ap_list[i].bssid[1],
                          result->ap_list[i].bssid[2],
                          result->ap_list[i].bssid[3],
                          result->ap_list[i].bssid[4],
                          result->ap_list[i].bssid[5],
                          result->ap_list[i].rssi);
    }

    offset += snprintf(params + offset, 2048 - offset, "&coord=wgs84&output=json");

    ESP_LOGI(TAG, "请求参数: %s", params);

    app_http_get(LOCATION_URL, params, http_response_handler, NULL);

    free(params);
}
