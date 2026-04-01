#include "app_time.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_log.h"
#include <time.h>
#include <sys/time.h>

static const char* TAG = "app_time";
static bool s_time_synced = false;
static bool s_sntp_initialized = false;

static void time_sync_notification_cb(struct timeval *tv) {
    s_time_synced = true;
    ESP_LOGI(TAG, "时间同步完成");
}

esp_err_t app_time_sntp_init(void) {
    if (s_sntp_initialized) {
        ESP_LOGW(TAG, "SNTP已经初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化SNTP");

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_setservername(1, "cn.pool.ntp.org");
    esp_sntp_setservername(2, "ntp1.aliyun.com");

    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);

    esp_sntp_init();

    setenv("TZ", "CST-8", 1);
    tzset();

    s_sntp_initialized = true;

    return ESP_OK;
}

esp_err_t app_time_sntp_sync(void) {
    if (!esp_sntp_enabled()) {
        ESP_LOGW(TAG, "SNTP未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    s_time_synced = false;

    esp_sntp_restart();

    return ESP_OK;
}

bool app_time_is_synced(void) {
    return s_time_synced;
}

esp_err_t app_time_get_now(app_time_t* time) {
    if (time == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    struct timeval tv;
    if (gettimeofday(&tv, NULL) != 0) {
        ESP_LOGE(TAG, "获取时间失败");
        return ESP_FAIL;
    }

    time_t now = tv.tv_sec;
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);

    time->year = timeinfo.tm_year + 1900;
    time->month = timeinfo.tm_mon + 1;
    time->day = timeinfo.tm_mday;
    time->hour = timeinfo.tm_hour;
    time->minute = timeinfo.tm_min;
    time->second = timeinfo.tm_sec;
    time->weekday = timeinfo.tm_wday;

    return ESP_OK;
}

void app_time_wait_sync(uint32_t timeout_ms) {
    uint32_t elapsed = 0;
    uint32_t check_interval = 100;

    while (!s_time_synced && elapsed < timeout_ms) {
        vTaskDelay(pdMS_TO_TICKS(check_interval));
        elapsed += check_interval;
    }

    if (s_time_synced) {
        ESP_LOGI(TAG, "时间同步成功，耗时 %u ms", elapsed);
    } else {
        ESP_LOGW(TAG, "时间同步超时");
    }
}
