#include "app_time.h"
#include "esp_sntp.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include <time.h>
#include <sys/time.h>
#include <stdio.h>
#include <math.h>

static const char* TAG = "app_time";
static bool s_time_synced = false;
static bool s_sntp_initialized = false;
static int s_timezone_offset = 8;

typedef struct {
    int64_t timestamp_us;
    int64_t sleep_enter_time_us;
    bool valid;
} rtc_time_backup_t;

static RTC_DATA_ATTR rtc_time_backup_t s_rtc_time_backup = {0};

/* 根据当前时区偏移量设置系统TZ环境变量 */
static void apply_timezone(void)
{
    char tz[32];
    if (s_timezone_offset >= 0) {
        snprintf(tz, sizeof(tz), "CST-%d", s_timezone_offset);
    } else {
        snprintf(tz, sizeof(tz), "CST%d", s_timezone_offset);
    }
    setenv("TZ", tz, 1);
    tzset();
    ESP_LOGI(TAG, "时区设置为 UTC%+d (TZ=%s)", s_timezone_offset, tz);
}

/* SNTP时间同步成功通知回调 */
static void time_sync_notification_cb(struct timeval *tv) {
    s_time_synced = true;
    ESP_LOGI(TAG, "时间同步完成");
}

/**
 * @brief 初始化SNTP时间同步服务（配置NTP服务器和时区）
 */
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

    apply_timezone();

    s_sntp_initialized = true;

    return ESP_OK;
}

/**
 * @brief 重启SNTP并开始新一轮时间同步
 */
esp_err_t app_time_sntp_sync(void) {
    if (!esp_sntp_enabled()) {
        ESP_LOGW(TAG, "SNTP未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    s_time_synced = false;

    esp_sntp_restart();

    return ESP_OK;
}

/* 查询SNTP是否已完成时间同步 */
bool app_time_is_synced(void) {
    return s_time_synced;
}

/**
 * @brief 获取当前系统时间（带校验：年份必须 >= 2020）
 */
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

    if (time->year < 2020) {
        return ESP_FAIL;
    }

    time->month = timeinfo.tm_mon + 1;
    time->day = timeinfo.tm_mday;
    time->hour = timeinfo.tm_hour;
    time->minute = timeinfo.tm_min;
    time->second = timeinfo.tm_sec;
    time->weekday = timeinfo.tm_wday;

    return ESP_OK;
}

/**
 * @brief 阻塞等待SNTP同步完成（直到成功或超时）
 */
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

/**
 * @brief 保存当前时间到RTC（深度睡眠后由底层RTC自动维护）
 */
esp_err_t app_time_save_to_rtc(void) {
    ESP_LOGI(TAG, "系统时间将由底层 RTC 自动维护");
    return ESP_OK;
}

/**
 * @brief 从RTC恢复时区设置，读取当前RTC时间
 */
esp_err_t app_time_restore_from_rtc(void) {
    apply_timezone();

    app_time_t time;
    if (app_time_get_now(&time) == ESP_OK) {
        ESP_LOGI(TAG, "系统时区已恢复，当前 RTC 时间: %04d-%02d-%02d %02d:%02d:%02d",
                 time.year, time.month, time.day,
                 time.hour, time.minute, time.second);
    }
    return ESP_OK;
}

/* 检查RTC内存中是否有有效的时间备份 */
bool app_time_has_rtc_backup(void) {
    return s_rtc_time_backup.valid;
}

/**
 * @brief 根据经度计算并设置时区（每15度经度对应1小时偏移）
 */
void app_time_set_timezone(double longitude)
{
    int offset = (int)round(longitude / 15.0);
    if (offset < -12) offset = -12;
    if (offset > 14) offset = 14;

    if (offset == s_timezone_offset) {
        ESP_LOGI(TAG, "时区未变化: UTC%+d", offset);
        return;
    }

    ESP_LOGI(TAG, "根据经度 %.2f 计算时区: UTC%+d", longitude, offset);
    s_timezone_offset = offset;
    apply_timezone();
}
