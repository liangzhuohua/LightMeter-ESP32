#include "app_http_requests.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"

static const char* TAG = "app_http_requests";

typedef struct {
    char *data;
    size_t len;
    size_t size;
    http_response_callback_t callback;
    void* user_data;
} http_response_t;

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_response_t *response = (http_response_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (response->len + evt->data_len < response->size - 1) {
                    memcpy(response->data + response->len, evt->data, evt->data_len);
                    response->len += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            if (response->len > 0) {
                response->data[response->len] = '\0';
                ESP_LOGI(TAG, "Response: %s", response->data);
                if (response->callback) {
                    response->callback(response->data, response->len, response->user_data);
                }
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGI(TAG, "HTTP_EVENT_REDIRECT");
            break;
    }
    return ESP_OK;
}

esp_err_t app_http_get(const char* url, const char* params, http_response_callback_t callback, void* user_data) {
    ESP_LOGI(TAG, "HTTP GET 请求: %s?%s", url, params);

    size_t url_len = strlen(url);
    size_t params_len = strlen(params);
    char *full_url = (char *)malloc(url_len + params_len + 2);
    if (full_url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return ESP_FAIL;
    }
    snprintf(full_url, url_len + params_len + 2, "%s?%s", url, params);

    char *response_buffer = (char *)malloc(8192);
    if (response_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        free(full_url);
        return ESP_FAIL;
    }

    http_response_t response = {
        .data = response_buffer,
        .len = 0,
        .size = 8192,
        .callback = callback,
        .user_data = user_data
    };

    esp_http_client_config_t config = {
        .url = full_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .buffer_size = 8192,
        .event_handler = http_event_handler,
        .user_data = &response,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(full_url);
        free(response_buffer);
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        ESP_LOGI(TAG, "HTTP GET Status = %d, content_length = %d", status_code, content_length);

        if (status_code >= 200 && status_code < 300) {
            ESP_LOGI(TAG, "HTTP request completed successfully");
            err = ESP_OK;
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(response_buffer);
    free(full_url);
    return err;
}

typedef struct {
    char *data;
    size_t len;
    size_t size;
} http_sync_internal_t;

static esp_err_t http_sync_event_handler(esp_http_client_event_t *evt) {
    http_sync_internal_t *resp = (http_sync_internal_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (!esp_http_client_is_chunked_response(evt->client)) {
                if (resp->len + evt->data_len < resp->size - 1) {
                    memcpy(resp->data + resp->len, evt->data, evt->data_len);
                    resp->len += evt->data_len;
                }
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (resp->len > 0) {
                resp->data[resp->len] = '\0';
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t app_http_get_sync(const char* url, const char* params, const char* auth_header, http_sync_response_t* response) {
    ESP_LOGI(TAG, "HTTP GET sync: %s?%s", url, params);

    size_t url_len = strlen(url);
    size_t params_len = strlen(params);
    char *full_url = (char *)malloc(url_len + params_len + 2);
    if (full_url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return ESP_FAIL;
    }
    snprintf(full_url, url_len + params_len + 2, "%s?%s", url, params);

    char *response_buffer = (char *)malloc(8192);
    if (response_buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        free(full_url);
        return ESP_FAIL;
    }

    http_sync_internal_t internal_resp = {
        .data = response_buffer,
        .len = 0,
        .size = 8192,
    };

    esp_http_client_config_t config = {
        .url = full_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 10000,
        .buffer_size = 8192,
        .event_handler = http_sync_event_handler,
        .user_data = &internal_resp,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(full_url);
        free(response_buffer);
        return ESP_FAIL;
    }

    if (auth_header != NULL) {
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        ESP_LOGI(TAG, "HTTP GET sync Status = %d, len = %d", status_code, internal_resp.len);

        if (status_code >= 200 && status_code < 300) {
            response->data = response_buffer;
            response->len = internal_resp.len;
            free(full_url);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
            err = ESP_FAIL;
        }
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    free(response_buffer);
    free(full_url);
    return err;
}
