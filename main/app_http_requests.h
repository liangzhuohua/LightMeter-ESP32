#ifndef __APP_HTTP_REQUESTS_H__
#define __APP_HTTP_REQUESTS_H__

#include <esp_err.h>

typedef void (*http_response_callback_t)(const char* response, int len, void* user_data);

typedef struct {
    char *data;
    size_t len;
} http_sync_response_t;

typedef struct {
    const char* key;
    const char* value;
} http_header_t;

esp_err_t app_http_get(const char* url, const char* params, http_response_callback_t callback, void* user_data);
esp_err_t app_http_get_sync(const char* url, const char* params, const char* auth_header, http_sync_response_t* response);
esp_err_t app_http_get_with_headers(const char* url, const char* params, const http_header_t* headers, int header_count, http_sync_response_t* response);

void app_http_free_response(http_sync_response_t* response);

#endif
