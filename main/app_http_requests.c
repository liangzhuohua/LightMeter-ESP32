#include "app_http_requests.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "miniz.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "app_http_requests";

#define HTTP_MAX_RETRIES 3
#define HTTP_RETRY_DELAY_MS 1000

typedef struct {
    char *data;
    size_t len;
    size_t size;
    http_response_callback_t callback;
    void* user_data;
    bool callback_called;
} http_response_t;


// ==================== Gzip 解压函数 (使用 tinfl_decompress，支持 Gzip 格式) ====================
static char* decompress_gzip(const uint8_t* compressed, size_t compressed_len, size_t* out_len)
{
    if (compressed == NULL || compressed_len < 18 || out_len == NULL) {
        return NULL;
    }

    // 检查 Gzip 魔术头
    if (compressed[0] != 0x1F || compressed[1] != 0x8B) {
        ESP_LOGW(TAG, "Not gzip compressed data");
        return NULL;
    }

    // 检查压缩方法（必须是 deflate，值为 8）
    if (compressed[2] != 8) {
        ESP_LOGW(TAG, "Unsupported compression method: %d", compressed[2]);
        return NULL;
    }

    uint8_t flags = compressed[3];
    size_t header_size = 10;

    // 如果设置了 FEXTRA 标志，跳过额外字段
    if (flags & 0x04) {
        if (header_size + 2 > compressed_len) {
            return NULL;
        }
        uint16_t xlen = compressed[header_size] | (compressed[header_size + 1] << 8);
        header_size += 2 + xlen;
    }

    // 如果设置了 FNAME 标志，跳过原始文件名
    if (flags & 0x08) {
        while (header_size < compressed_len && compressed[header_size] != 0) {
            header_size++;
        }
        header_size++; // 跳过 null 终止符
    }

    // 如果设置了 FCOMMENT 标志，跳过注释
    if (flags & 0x10) {
        while (header_size < compressed_len && compressed[header_size] != 0) {
            header_size++;
        }
        header_size++; // 跳过 null 终止符
    }

    // 如果设置了 FHCRC 标志，跳过头部 CRC16
    if (flags & 0x02) {
        header_size += 2;
    }

    // 检查是否有足够的数据（头部 + 8字节尾部）
    if (header_size >= compressed_len - 8) {
        ESP_LOGW(TAG, "Invalid gzip data: header too large");
        return NULL;
    }

    // 压缩数据从 header_size 开始，到 compressed_len - 8 结束
    // 最后 8 字节是 CRC32 和原始大小（未使用）
    const uint8_t* deflate_data = compressed + header_size;
    size_t deflate_len = compressed_len - header_size - 8;

    ESP_LOGI(TAG, "Gzip header: flags=0x%02X, header_size=%d, deflate_len=%d",
             flags, (int)header_size, (int)deflate_len);

    // 预分配解压缓冲区（天气JSON通常不大，预估 10 倍大小）
    size_t decomp_buf_size = compressed_len * 10;
    char* decomp_buf = (char*)malloc(decomp_buf_size);
    if (decomp_buf == NULL) {
        ESP_LOGE(TAG, "Failed to allocate decompression buffer");
        return NULL;
    }

    // 使用底层的 tinfl_decompress 函数，更可控
    tinfl_decompressor decomp;
    tinfl_init(&decomp);

    size_t in_bytes = deflate_len;
    size_t out_bytes = decomp_buf_size - 1;
    tinfl_status status = tinfl_decompress(
        &decomp,
        deflate_data,
        &in_bytes,
        (uint8_t*)decomp_buf,
        (uint8_t*)decomp_buf,
        &out_bytes,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF
    );

    if (status != TINFL_STATUS_DONE) {
        ESP_LOGE(TAG, "tinfl_decompress failed with status: %d", status);
        free(decomp_buf);
        return NULL;
    }

    // 添加字符串终止符
    decomp_buf[out_bytes] = '\0';
    *out_len = out_bytes;

    ESP_LOGI(TAG, "✅ Gzip 解压成功: %d bytes → %d bytes", (int)compressed_len, (int)*out_len);
    return decomp_buf;
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt) {
    http_response_t *response = (http_response_t *)evt->user_data;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            if (response && response->callback && !response->callback_called) {
                response->callback_called = true;
                response->callback(NULL, 0, response->user_data);
            }
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
            if (response && response->callback && !response->callback_called) {
                if (response->len > 0) {
                    response->data[response->len] = '\0';
                    ESP_LOGI(TAG, "Response: %s", response->data);
                    response->callback_called = true;
                    response->callback(response->data, response->len, response->user_data);
                } else {
                    response->callback_called = true;
                    response->callback(NULL, 0, response->user_data);
                }
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            if (response && response->callback && !response->callback_called) {
                response->callback_called = true;
                response->callback(NULL, 0, response->user_data);
            }
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
        .user_data = user_data,
        .callback_called = false
    };

    esp_http_client_config_t config = {
        .url = full_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .buffer_size = 8192,
        .event_handler = http_event_handler,
        .user_data = &response,
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
                // 自动检测并解压 Gzip
        if (resp->len > 2 &&
            (uint8_t)resp->data[0] == 0x1F &&
            (uint8_t)resp->data[1] == 0x8B) {

            ESP_LOGI(TAG, "检测到 Gzip 压缩数据，正在解压...");
            size_t new_len = 0;
            char* new_data = decompress_gzip((uint8_t*)resp->data, resp->len, &new_len);

            if (new_data) {
                free(resp->data);
                resp->data = new_data;
                resp->len = new_len;
            }
        }
            }
            break;
        default:
            break;
    }
    return ESP_OK;
}

void app_http_free_response(http_sync_response_t* response) {
    if (response != NULL && response->data != NULL) {
        free(response->data);
        response->data = NULL;
        response->len = 0;
    }
}

esp_err_t app_http_get_with_headers(const char* url, const char* params, const http_header_t* headers, int header_count, http_sync_response_t* response) {
    ESP_LOGI(TAG, "HTTP GET with headers: %s?%s", url, params);

    size_t url_len = strlen(url);
    size_t params_len = strlen(params);
    char *full_url = (char *)malloc(url_len + params_len + 2);
    if (full_url == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for URL");
        return ESP_FAIL;
    }
    snprintf(full_url, url_len + params_len + 2, "%s?%s", url, params);

    esp_err_t err = ESP_FAIL;
    int retry_count = 0;

    while (retry_count < HTTP_MAX_RETRIES) {
        if (retry_count > 0) {
            ESP_LOGI(TAG, "重试第 %d/%d 次...", retry_count, HTTP_MAX_RETRIES);
            vTaskDelay(pdMS_TO_TICKS(HTTP_RETRY_DELAY_MS));
        }

        char *response_buffer = (char *)malloc(16384);
        if (response_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate memory for response");
            free(full_url);
            return ESP_FAIL;
        }

        http_sync_internal_t internal_resp = {
            .data = response_buffer,
            .len = 0,
            .size = 16384,
        };

        esp_http_client_config_t config = {
            .url = full_url,
            .method = HTTP_METHOD_GET,
            .timeout_ms = 15000,
            .buffer_size = 16384,
            .event_handler = http_sync_event_handler,
            .user_data = &internal_resp,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };

        esp_http_client_handle_t client = esp_http_client_init(&config);
        if (client == NULL) {
            ESP_LOGE(TAG, "Failed to initialize HTTP client");
            free(response_buffer);
            retry_count++;
            continue;
        }

        for (int i = 0; i < header_count; i++) {
            if (headers[i].key != NULL && headers[i].value != NULL) {
                esp_http_client_set_header(client, headers[i].key, headers[i].value);
            }
        }

        err = esp_http_client_perform(client);
        if (err == ESP_OK) {
            int status_code = esp_http_client_get_status_code(client);
            ESP_LOGI(TAG, "HTTP GET with headers Status = %d, len = %d", status_code, internal_resp.len);

            if (status_code >= 200 && status_code < 300) {
                response->data = internal_resp.data;
                response->len = internal_resp.len;
                free(full_url);
                esp_http_client_cleanup(client);
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "HTTP request failed with status code: %d", status_code);
                err = ESP_FAIL;
            }
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        }

        esp_http_client_cleanup(client);
        free(internal_resp.data);
        retry_count++;
    }

    ESP_LOGE(TAG, "HTTP请求失败，已重试 %d 次", HTTP_MAX_RETRIES);
    free(full_url);
    return err;
}
