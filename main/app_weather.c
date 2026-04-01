#include "app_weather.h"
#include "app_http_requests.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include "mbedtls/base64.h"
#include "psa/crypto.h"
#include <string.h>
#include <time.h>

static const char* TAG = "app_weather";

#define ALG                 "EdDSA"
#define KID                 "TMPKUUQFWQ"
#define SUB                 "3H86T96TAB"
#define WEATHER_API_HOST    "m43dn9h9jc.re.qweatherapi.com"

static const uint8_t ed25519_private_key_seed[] = {
    0xfe, 0x7c, 0x68, 0x3b, 0x43, 0xe0, 0xb6, 0x98,
    0x80, 0x8d, 0x55, 0xa1, 0x06, 0x67, 0xbb, 0x91,
    0x0c, 0x2e, 0x1f, 0xbe, 0x5e, 0x00, 0xdb, 0x6c,
    0x20, 0x4b, 0xf9, 0x7e, 0x20, 0x91, 0x55, 0xcd
};

static char* base64url_encode(const uint8_t* data, size_t len) {
    size_t out_len;
    mbedtls_base64_encode(NULL, 0, &out_len, data, len);

    char* encoded = malloc(out_len + 1);
    if (encoded == NULL) return NULL;

    mbedtls_base64_encode((unsigned char*)encoded, out_len, &out_len, data, len);
    encoded[out_len] = '\0';

    for (size_t i = 0; i < out_len; i++) {
        if (encoded[i] == '+') encoded[i] = '-';
        else if (encoded[i] == '/') encoded[i] = '_';
        else if (encoded[i] == '=') encoded[i] = '\0';
    }

    return encoded;
}

static esp_err_t generate_jwt(char* jwt_out, size_t jwt_max_len) {
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;

    int64_t now = esp_timer_get_time() / 1000000;
    int64_t iat = now - 30;
    int64_t exp = now + 3600;

    char header[128];
    char payload[128];
    snprintf(header, sizeof(header), "{\"alg\":\"%s\",\"kid\":\"%s\"}", ALG, KID);
    snprintf(payload, sizeof(payload), "{\"sub\":\"%s\",\"iat\":%lld,\"exp\":%lld}", SUB, iat, exp);

    char* header_b64 = base64url_encode((uint8_t*)header, strlen(header));
    char* payload_b64 = base64url_encode((uint8_t*)payload, strlen(payload));

    if (header_b64 == NULL || payload_b64 == NULL) {
        free(header_b64);
        free(payload_b64);
        return ESP_ERR_NO_MEM;
    }

    char signing_input[512];
    snprintf(signing_input, sizeof(signing_input), "%s.%s", header_b64, payload_b64);

    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_PURE_EDDSA);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_ECC_KEY_PAIR(PSA_ECC_FAMILY_TWISTED_EDWARDS));
    psa_set_key_bits(&attributes, 256);

    status = psa_import_key(&attributes, ed25519_private_key_seed, 32, &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_import_key failed: %d", status);
        free(header_b64);
        free(payload_b64);
        return ESP_FAIL;
    }

    uint8_t signature[64];
    size_t sig_len;
    status = psa_sign_message(key_id, PSA_ALG_PURE_EDDSA,
                              (uint8_t*)signing_input, strlen(signing_input),
                              signature, sizeof(signature), &sig_len);

    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_sign_message failed: %d", status);
        free(header_b64);
        free(payload_b64);
        return ESP_FAIL;
    }

    char* sig_b64 = base64url_encode(signature, sig_len);
    if (sig_b64 == NULL) {
        free(header_b64);
        free(payload_b64);
        return ESP_ERR_NO_MEM;
    }

    snprintf(jwt_out, jwt_max_len, "%s.%s.%s", header_b64, payload_b64, sig_b64);

    free(header_b64);
    free(payload_b64);
    free(sig_b64);

    return ESP_OK;
}

static void parse_weather_response(const char* json_data, weather_result_callback_t callback) {
    if (json_data == NULL || callback == NULL) return;

    cJSON* root = cJSON_Parse(json_data);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse failed");
        return;
    }

    weather_data_t weather = {0};

    cJSON* now = cJSON_GetObjectItem(root, "now");
    if (now != NULL) {
        cJSON* temp = cJSON_GetObjectItem(now, "temp");
        cJSON* text = cJSON_GetObjectItem(now, "text");
        cJSON* humidity = cJSON_GetObjectItem(now, "humidity");
        cJSON* windSpeed = cJSON_GetObjectItem(now, "windSpeed");

        if (temp) weather.temp = atoi(temp->valuestring);
        if (text) strncpy(weather.desc, text->valuestring, sizeof(weather.desc) - 1);
        if (humidity) weather.humidity = atoi(humidity->valuestring);
        if (windSpeed) weather.wind_speed = atof(windSpeed->valuestring);
    }

    cJSON* daily = cJSON_GetObjectItem(root, "daily");
    if (daily != NULL && cJSON_IsArray(daily)) {
        cJSON* first_day = cJSON_GetArrayItem(daily, 0);
        if (first_day != NULL) {
            cJSON* sunrise = cJSON_GetObjectItem(first_day, "sunrise");
            cJSON* sunset = cJSON_GetObjectItem(first_day, "sunset");

            if (sunrise && strlen(sunrise->valuestring) >= 5) {
                weather.sunrise_hour = atoi(sunrise->valuestring);
                weather.sunrise_minute = atoi(sunrise->valuestring + 3);
            }
            if (sunset && strlen(sunset->valuestring) >= 5) {
                weather.sunset_hour = atoi(sunset->valuestring);
                weather.sunset_minute = atoi(sunset->valuestring + 3);
            }
        }
    }

    cJSON_Delete(root);
    callback(&weather);
}

esp_err_t app_weather_init(void) {
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "psa_crypto_init failed: %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t app_weather_get(const char* location, weather_result_callback_t callback) {
    if (location == NULL) {
        ESP_LOGE(TAG, "location is NULL");
        return ESP_ERR_INVALID_ARG;
    }

    char jwt[1024];
    esp_err_t ret = generate_jwt(jwt, sizeof(jwt));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to generate JWT");
        return ret;
    }

    ESP_LOGI(TAG, "JWT: %s", jwt);

    char path_now[256];
    char path_3d[256];
    snprintf(path_now, sizeof(path_now), "/v7/weather/now?location=%s", location);
    snprintf(path_3d, sizeof(path_3d), "/v7/weather/3d?location=%s", location);

    char auth_header[1152];
    snprintf(auth_header, sizeof(auth_header), "Bearer %s", jwt);

    http_sync_response_t response_now = {0};
    ret = app_http_get_sync(WEATHER_API_HOST, path_now, auth_header, &response_now);

    if (ret != ESP_OK || response_now.data == NULL) {
        ESP_LOGE(TAG, "HTTP request for now failed");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Weather now response: %s", response_now.data);

    http_sync_response_t response_3d = {0};
    ret = app_http_get_sync(WEATHER_API_HOST, path_3d, auth_header, &response_3d);

    if (ret != ESP_OK || response_3d.data == NULL) {
        ESP_LOGE(TAG, "HTTP request for 3d failed");
        free(response_now.data);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Weather 3d response: %s", response_3d.data);

    cJSON* root_now = cJSON_Parse(response_now.data);
    cJSON* root_3d = cJSON_Parse(response_3d.data);

    weather_data_t weather = {0};

    if (root_now != NULL) {
        cJSON* now = cJSON_GetObjectItem(root_now, "now");
        if (now != NULL) {
            cJSON* temp = cJSON_GetObjectItem(now, "temp");
            cJSON* text = cJSON_GetObjectItem(now, "text");
            cJSON* humidity = cJSON_GetObjectItem(now, "humidity");
            cJSON* windSpeed = cJSON_GetObjectItem(now, "windSpeed");

            if (temp) weather.temp = atoi(temp->valuestring);
            if (text) strncpy(weather.desc, text->valuestring, sizeof(weather.desc) - 1);
            if (humidity) weather.humidity = atoi(humidity->valuestring);
            if (windSpeed) weather.wind_speed = atof(windSpeed->valuestring);
        }
        cJSON_Delete(root_now);
    }

    if (root_3d != NULL) {
        cJSON* daily = cJSON_GetObjectItem(root_3d, "daily");
        if (daily != NULL && cJSON_IsArray(daily)) {
            cJSON* first_day = cJSON_GetArrayItem(daily, 0);
            if (first_day != NULL) {
                cJSON* sunrise = cJSON_GetObjectItem(first_day, "sunrise");
                cJSON* sunset = cJSON_GetObjectItem(first_day, "sunset");

                if (sunrise && strlen(sunrise->valuestring) >= 5) {
                    weather.sunrise_hour = atoi(sunrise->valuestring);
                    weather.sunrise_minute = atoi(sunrise->valuestring + 3);
                }
                if (sunset && strlen(sunset->valuestring) >= 5) {
                    weather.sunset_hour = atoi(sunset->valuestring);
                    weather.sunset_minute = atoi(sunset->valuestring + 3);
                }
            }
        }
        cJSON_Delete(root_3d);
    }

    free(response_now.data);
    free(response_3d.data);

    if (callback != NULL) {
        callback(&weather);
    }

    return ESP_OK;
}
