#ifndef __APP_HTTP_REQUESTS_H__
#define __APP_HTTP_REQUESTS_H__

#include <esp_err.h>

typedef void (*http_response_callback_t)(const char* response, int len, void* user_data);

esp_err_t app_http_get(const char* url, const char* params, http_response_callback_t callback, void* user_data);

#endif
