#include "hw_nvs.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>

static const char *TAG = "hw_nvs";

/* 打开指定命名空间的NVS句柄 */
static int open_nvs_handle(const char *namespace, nvs_open_mode_t mode, nvs_handle_t *out_handle)
{
    if (!namespace || !out_handle) {
        return -1;
    }

    esp_err_t ret = nvs_open(namespace, mode, out_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed for namespace '%s': %s", namespace, esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int hw_nvs_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK && ret != ESP_ERR_NVS_INVALID_STATE) {
        ESP_LOGE(TAG, "nvs_flash_init failed: %s", esp_err_to_name(ret));
        return -1;
    }

    ESP_LOGI(TAG, "NVS init OK");
    return 0;
}

int hw_nvs_set_bool(const char *namespace, const char *key, bool value)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READWRITE, &handle) != 0) {
        return -1;
    }

    esp_err_t ret = nvs_set_u8(handle, key, value ? 1 : 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_u8 failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return -1;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int hw_nvs_get_bool(const char *namespace, const char *key, bool *value)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READONLY, &handle) != 0) {
        return -1;
    }

    uint8_t temp;
    esp_err_t ret = nvs_get_u8(handle, key, &temp);
    nvs_close(handle);

    if (ret != ESP_OK) {
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "nvs_get_u8 failed: %s", esp_err_to_name(ret));
        }
        return -1;
    }

    if (value) {
        *value = (temp != 0);
    }

    return 0;
}

int hw_nvs_set_int(const char *namespace, const char *key, int32_t value)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READWRITE, &handle) != 0) {
        return -1;
    }

    esp_err_t ret = nvs_set_i32(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_i32 failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return -1;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int hw_nvs_get_int(const char *namespace, const char *key, int32_t *value)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READONLY, &handle) != 0) {
        return -1;
    }

    int32_t temp;
    esp_err_t ret = nvs_get_i32(handle, key, &temp);
    nvs_close(handle);

    if (ret != ESP_OK) {
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "nvs_get_i32 failed: %s", esp_err_to_name(ret));
        }
        return -1;
    }

    if (value) {
        *value = temp;
    }

    return 0;
}

int hw_nvs_set_i64(const char *namespace, const char *key, int64_t value)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READWRITE, &handle) != 0) {
        return -1;
    }

    esp_err_t ret = nvs_set_i64(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_i64 failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return -1;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int hw_nvs_get_i64(const char *namespace, const char *key, int64_t *value)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READONLY, &handle) != 0) {
        return -1;
    }

    int64_t temp;
    esp_err_t ret = nvs_get_i64(handle, key, &temp);
    nvs_close(handle);

    if (ret != ESP_OK) {
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "nvs_get_i64 failed: %s", esp_err_to_name(ret));
        }
        return -1;
    }

    if (value) {
        *value = temp;
    }

    return 0;
}

int hw_nvs_set_string(const char *namespace, const char *key, const char *value)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READWRITE, &handle) != 0) {
        return -1;
    }

    esp_err_t ret = nvs_set_str(handle, key, value);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_str failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return -1;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int hw_nvs_get_string(const char *namespace, const char *key, char *value, size_t *len)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READONLY, &handle) != 0) {
        return -1;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_str(handle, key, NULL, &required_size);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "nvs_get_str (size query) failed: %s", esp_err_to_name(ret));
        }
        nvs_close(handle);
        return -1;
    }

    if (value == NULL || *len < required_size) {
        if (len) {
            *len = required_size;
        }
        nvs_close(handle);
        return -1;
    }

    ret = nvs_get_str(handle, key, value, &required_size);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_str failed: %s", esp_err_to_name(ret));
        return -1;
    }

    if (len) {
        *len = required_size;
    }

    return 0;
}

int hw_nvs_set_blob(const char *namespace, const char *key, const void *value, size_t len)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READWRITE, &handle) != 0) {
        return -1;
    }

    esp_err_t ret = nvs_set_blob(handle, key, value, len);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_set_blob failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return -1;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int hw_nvs_get_blob(const char *namespace, const char *key, void *value, size_t *len)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READONLY, &handle) != 0) {
        return -1;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_blob(handle, key, NULL, &required_size);
    if (ret != ESP_OK) {
        if (ret != ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGE(TAG, "nvs_get_blob (size query) failed: %s", esp_err_to_name(ret));
        }
        nvs_close(handle);
        return -1;
    }

    if (value == NULL || *len < required_size) {
        if (len) {
            *len = required_size;
        }
        nvs_close(handle);
        return -1;
    }

    ret = nvs_get_blob(handle, key, value, &required_size);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_get_blob failed: %s", esp_err_to_name(ret));
        return -1;
    }

    if (len) {
        *len = required_size;
    }

    return 0;
}

int hw_nvs_erase_key(const char *namespace, const char *key)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READWRITE, &handle) != 0) {
        return -1;
    }

    esp_err_t ret = nvs_erase_key(handle, key);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_erase_key failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return -1;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

int hw_nvs_erase_namespace(const char *namespace)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READWRITE, &handle) != 0) {
        return -1;
    }

    esp_err_t ret = nvs_erase_all(handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_erase_all failed: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return -1;
    }

    ret = nvs_commit(handle);
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nvs_commit failed: %s", esp_err_to_name(ret));
        return -1;
    }

    return 0;
}

bool hw_nvs_key_exists(const char *namespace, const char *key)
{
    nvs_handle_t handle;
    if (open_nvs_handle(namespace, NVS_READONLY, &handle) != 0) {
        return false;
    }

    size_t required_size = 0;
    esp_err_t ret = nvs_get_str(handle, key, NULL, &required_size);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ret = nvs_get_blob(handle, key, NULL, &required_size);
    }
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        uint8_t temp;
        ret = nvs_get_u8(handle, key, &temp);
    }
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        int32_t temp;
        ret = nvs_get_i32(handle, key, &temp);
    }
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        int64_t temp;
        ret = nvs_get_i64(handle, key, &temp);
    }

    nvs_close(handle);
    return (ret != ESP_ERR_NVS_NOT_FOUND);
}
