#include "app_weather.h"
#include "app_http_requests.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "app_weather";

#define WEATHER_API_KEY         "75fee4aab17544ecaf6f4ef3f8559864"
#define WEATHER_API_HOST        "m43dn9h9jc.re.qweatherapi.com"
#define WEATHER_API_PATH_NOW    "/v7/weather/now"
#define WEATHER_API_PATH_3D     "/v7/weather/3d"

static bool g_weather_initialized = false;

typedef void (*weather_callback_t)(const weather_data_t* data);

static esp_err_t parse_weather_now_response(const char* json_str, weather_data_t* data) {
    if (json_str == NULL || data == NULL) {
        return ESP_FAIL;
    }

    cJSON* root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_FAIL;
    }

    cJSON* code = cJSON_GetObjectItem(root, "code");
    if (code == NULL || strcmp(code->valuestring, "200") != 0) {
        ESP_LOGE(TAG, "API returned error code: %s", code ? code->valuestring : "null");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON* now = cJSON_GetObjectItem(root, "now");
    if (now == NULL) {
        ESP_LOGE(TAG, "No 'now' field in response");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON* temp = cJSON_GetObjectItem(now, "temp");
    if (temp != NULL) {
        data->temp = atoi(temp->valuestring);
    }

    cJSON* text = cJSON_GetObjectItem(now, "text");
    if (text != NULL) {
        strncpy(data->desc, text->valuestring, sizeof(data->desc) - 1);
        data->desc[sizeof(data->desc) - 1] = '\0';
    }

    cJSON* icon = cJSON_GetObjectItem(now, "icon");
    if (icon != NULL) {
        strncpy(data->icon, icon->valuestring, sizeof(data->icon) - 1);
        data->icon[sizeof(data->icon) - 1] = '\0';
    }

    cJSON* humidity = cJSON_GetObjectItem(now, "humidity");
    if (humidity != NULL) {
        data->humidity = atoi(humidity->valuestring);
    }

    cJSON* windSpeed = cJSON_GetObjectItem(now, "windSpeed");
    if (windSpeed != NULL) {
        data->wind_speed = atof(windSpeed->valuestring);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

static esp_err_t parse_weather_3d_response(const char* json_str, weather_data_t* data) {
    if (json_str == NULL || data == NULL) {
        return ESP_FAIL;
    }

    cJSON* root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON for 3d weather");
        return ESP_FAIL;
    }

    cJSON* code = cJSON_GetObjectItem(root, "code");
    if (code == NULL || strcmp(code->valuestring, "200") != 0) {
        ESP_LOGE(TAG, "API returned error code: %s", code ? code->valuestring : "null");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON* daily = cJSON_GetObjectItem(root, "daily");
    if (daily == NULL || cJSON_GetArraySize(daily) == 0) {
        ESP_LOGE(TAG, "No 'daily' field in response");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON* today = cJSON_GetArrayItem(daily, 0);
    if (today == NULL) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON* tempMax = cJSON_GetObjectItem(today, "tempMax");
    if (tempMax != NULL) {
        data->temp_max = atoi(tempMax->valuestring);
    }

    cJSON* tempMin = cJSON_GetObjectItem(today, "tempMin");
    if (tempMin != NULL) {
        data->temp_min = atoi(tempMin->valuestring);
    }

    cJSON* sunrise = cJSON_GetObjectItem(today, "sunrise");
    if (sunrise != NULL) {
        int hour, minute;
        if (sscanf(sunrise->valuestring, "%d:%d", &hour, &minute) == 2) {
            data->sunrise_hour = hour;
            data->sunrise_minute = minute;
        }
    }

    cJSON* sunset = cJSON_GetObjectItem(today, "sunset");
    if (sunset != NULL) {
        int hour, minute;
        if (sscanf(sunset->valuestring, "%d:%d", &hour, &minute) == 2) {
            data->sunset_hour = hour;
            data->sunset_minute = minute;
        }
    }

    cJSON* moonrise = cJSON_GetObjectItem(today, "moonrise");
    if (moonrise != NULL && strlen(moonrise->valuestring) > 0) {
        int hour, minute;
        if (sscanf(moonrise->valuestring, "%d:%d", &hour, &minute) == 2) {
            data->moonrise_hour = hour;
            data->moonrise_minute = minute;
        }
    }

    cJSON* moonset = cJSON_GetObjectItem(today, "moonset");
    if (moonset != NULL && strlen(moonset->valuestring) > 0) {
        int hour, minute;
        if (sscanf(moonset->valuestring, "%d:%d", &hour, &minute) == 2) {
            data->moonset_hour = hour;
            data->moonset_minute = minute;
        }
    }

    cJSON* moonPhase = cJSON_GetObjectItem(today, "moonPhase");
    if (moonPhase != NULL) {
        strncpy(data->moon_phase, moonPhase->valuestring, sizeof(data->moon_phase) - 1);
        data->moon_phase[sizeof(data->moon_phase) - 1] = '\0';
    }

    cJSON* moonPhaseIcon = cJSON_GetObjectItem(today, "moonPhaseIcon");
    if (moonPhaseIcon != NULL) {
        strncpy(data->moon_phase_icon, moonPhaseIcon->valuestring, sizeof(data->moon_phase_icon) - 1);
        data->moon_phase_icon[sizeof(data->moon_phase_icon) - 1] = '\0';
    }

    cJSON_Delete(root);
    return ESP_OK;
}

void app_weather_init(void) {
    if (g_weather_initialized) {
        ESP_LOGI(TAG, "Weather module already initialized");
        return;
    }

    ESP_LOGI(TAG, "Weather module initialized");
    g_weather_initialized = true;
}

void app_weather_get(const char* location_id, void (*callback)(const weather_data_t* data)) {
    if (location_id == NULL) {
        ESP_LOGE(TAG, "Location ID is NULL");
        return;
    }

    ESP_LOGI(TAG, "Getting weather for location: %s", location_id);

    http_sync_response_t response_now = {0};
    http_sync_response_t response_3d = {0};

    char url_now[256];
    snprintf(url_now, sizeof(url_now), "https://%s%s", WEATHER_API_HOST, WEATHER_API_PATH_NOW);

    char params_now[64];
    snprintf(params_now, sizeof(params_now), "location=%s", location_id);

    http_header_t headers[] = {
        {"X-QW-Api-Key", WEATHER_API_KEY}
    };

    esp_err_t err = app_http_get_with_headers(url_now, params_now, headers, 1, &response_now);
    if (err != ESP_OK || response_now.data == NULL) {
        ESP_LOGE(TAG, "Failed to get current weather");
        if (callback) {
            callback(NULL);
        }
        return;
    }

    ESP_LOGI(TAG, "Weather now response: %s", response_now.data);

    weather_data_t weather_data = {0};

    if (parse_weather_now_response(response_now.data, &weather_data) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse current weather response");
        app_http_free_response(&response_now);
        if (callback) {
            callback(NULL);
        }
        return;
    }

    app_http_free_response(&response_now);

    char url_3d[256];
    snprintf(url_3d, sizeof(url_3d), "https://%s%s", WEATHER_API_HOST, WEATHER_API_PATH_3D);

    err = app_http_get_with_headers(url_3d, params_now, headers, 1, &response_3d);
    if (err == ESP_OK && response_3d.data != NULL) {
        ESP_LOGI(TAG, "Weather 3d response: %s", response_3d.data);
        parse_weather_3d_response(response_3d.data, &weather_data);
        app_http_free_response(&response_3d);
    } else {
        ESP_LOGW(TAG, "Failed to get 3-day weather, using default sunrise/sunset");
        weather_data.sunrise_hour = 6;
        weather_data.sunrise_minute = 0;
        weather_data.sunset_hour = 18;
        weather_data.sunset_minute = 0;
    }

    ESP_LOGI(TAG, "Weather data parsed successfully");
    ESP_LOGI(TAG, "  Temperature: %d°C", weather_data.temp);
    ESP_LOGI(TAG, "  Description: %s", weather_data.desc);
    ESP_LOGI(TAG, "  Humidity: %d%%", weather_data.humidity);
    ESP_LOGI(TAG, "  Wind Speed: %.1f km/h", weather_data.wind_speed);
    ESP_LOGI(TAG, "  Sunrise: %02d:%02d", weather_data.sunrise_hour, weather_data.sunrise_minute);
    ESP_LOGI(TAG, "  Sunset: %02d:%02d", weather_data.sunset_hour, weather_data.sunset_minute);

    if (callback) {
        callback(&weather_data);
    }
}
