#include "hw_veml7700.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hw_veml7700";

i2c_dev_t veml7700_device;
veml7700_config_t veml7700_configuration;

/* ── 自适应量程 ─────────────────────────────────── */

/* 各级别对应的增益和积分时间 */
static const struct {
    uint16_t gain;
    uint16_t integration_time;
    const char *name;
} level_config[VEML7700_LEVEL_COUNT] = {
    [VEML7700_LEVEL_0] = {VEML7700_GAIN_2,     VEML7700_INTEGRATION_TIME_100MS, "DIM"},
    [VEML7700_LEVEL_1] = {VEML7700_GAIN_1,     VEML7700_INTEGRATION_TIME_100MS, "INDOOR"},
    [VEML7700_LEVEL_2] = {VEML7700_GAIN_DIV_8, VEML7700_INTEGRATION_TIME_100MS, "OUTDOOR"},
};

/* 切换阈值 */
#define RAW_HIGH_THRESHOLD  55000  /* 升档: raw 接近饱和 */
#define RAW_LOW_THRESHOLD   2000   /* 降档: raw 太低，分辨率不足 */

/* 切换稳定等待(ms) */
#define LEVEL_SWITCH_DELAY_MS  250

static veml7700_level_t current_level = VEML7700_LEVEL_1;

/* 逐级校准系数: calibrated = a * raw_lux^b */
static veml7700_calib_t calibration[VEML7700_LEVEL_COUNT] = {
    [VEML7700_LEVEL_0] = {0.8307f, 1.0753f},  /* GAIN_2,   100ms — 暗光 */
    [VEML7700_LEVEL_1] = {0.5615f, 1.1460f},  /* GAIN_1,   100ms — 室内 */
    [VEML7700_LEVEL_2] = {0.0613f, 1.3340f},  /* GAIN_1/8, 100ms — 户外 */
};

veml7700_level_t hw_veml7700_get_level(void)
{
    return current_level;
}

void hw_veml7700_set_calibration(veml7700_level_t level, float a, float b)
{
    if (level < VEML7700_LEVEL_COUNT) {
        calibration[level].a = a;
        calibration[level].b = b;
        ESP_LOGI(TAG, "Calib L%d: a=%.4f b=%.4f", level, a, b);
    }
}

static void apply_level_config(veml7700_level_t level)
{
    veml7700_configuration.gain = level_config[level].gain;
    veml7700_configuration.integration_time = level_config[level].integration_time;
    veml7700_set_config(&veml7700_device, &veml7700_configuration);
}

/* ── 初始化 / 关断 ──────────────────────────────── */

static bool compare_configuration(veml7700_config_t* config_a, veml7700_config_t* config_b)
{
    bool match = true;

    if (config_a->gain != config_b->gain) {
        ESP_LOGE(TAG, "gain mismatch: %d vs %d", config_a->gain, config_b->gain);
        match = false;
    }
    if (config_a->integration_time != config_b->integration_time) {
        ESP_LOGE(TAG, "integration_time mismatch: %d vs %d", config_a->integration_time, config_b->integration_time);
        match = false;
    }
    if (config_a->persistence_protect != config_b->persistence_protect) {
        ESP_LOGE(TAG, "persistence_protect mismatch: %d vs %d", config_a->persistence_protect, config_b->persistence_protect);
        match = false;
    }
    if (config_a->interrupt_enable != config_b->interrupt_enable) {
        ESP_LOGE(TAG, "interrupt_enable mismatch: %d vs %d", config_a->interrupt_enable, config_b->interrupt_enable);
        match = false;
    }
    if (config_a->shutdown != config_b->shutdown) {
        ESP_LOGE(TAG, "shutdown mismatch: %d vs %d", config_a->shutdown, config_b->shutdown);
        match = false;
    }
    if (config_a->power_saving_mode != config_b->power_saving_mode) {
        ESP_LOGE(TAG, "power_saving_mode mismatch: %d vs %d", config_a->power_saving_mode, config_b->power_saving_mode);
        match = false;
    }
    if (config_a->power_saving_enable != config_b->power_saving_enable) {
        ESP_LOGE(TAG, "power_saving_enable mismatch: %d vs %d", config_a->power_saving_enable, config_b->power_saving_enable);
        match = false;
    }

    return match;
}

void hw_veml7700_init(uint16_t gain, uint16_t integration_time, uint16_t power_saving_mode) {

    memset(&veml7700_device, 0, sizeof(i2c_dev_t));
    memset(&veml7700_configuration, 0, sizeof(veml7700_config_t));

    ESP_LOGI(TAG, "initializing hardware");

    ESP_ERROR_CHECK(veml7700_init_desc(&veml7700_device, HW_VEML7700_I2C_NUM, HW_VEML7700_SDA, HW_VEML7700_SCL));

    ESP_ERROR_CHECK(veml7700_probe(&veml7700_device));

    /* 用 Level 1 (中等增益) 作为初始配置 */
    current_level = VEML7700_LEVEL_1;
    veml7700_configuration.gain = level_config[current_level].gain;
    veml7700_configuration.integration_time = level_config[current_level].integration_time;

    veml7700_configuration.persistence_protect = VEML7700_PERSISTENCE_PROTECTION_4;
    veml7700_configuration.interrupt_enable = 1;
    veml7700_configuration.shutdown = 0;

    veml7700_configuration.power_saving_mode = power_saving_mode;
    veml7700_configuration.power_saving_enable = 1;

    ESP_ERROR_CHECK(veml7700_set_config(&veml7700_device, &veml7700_configuration));

    veml7700_config_t veml7700_configuration_readback;
    ESP_ERROR_CHECK(veml7700_get_config(&veml7700_device, &veml7700_configuration_readback));

    if (compare_configuration(&veml7700_configuration_readback,
                              &veml7700_configuration))
    {
        ESP_LOGI(TAG, "Configuration read back matches");
    }
    else
    {
        ESP_LOGE(TAG, "Configuration read back does not match");
    }
}

void hw_veml7700_get_ambient_light(uint32_t* als) {
    uint16_t raw;
    veml7700_level_t target_level = current_level;

    /* 1. 读当前量程下的 raw count */
    if (veml7700_get_als_raw(&veml7700_device, &raw) != ESP_OK) {
        *als = 0;
        return;
    }

    /* 2. 判断是否需要切换量程 */
    if (raw > RAW_HIGH_THRESHOLD && current_level < VEML7700_LEVEL_COUNT - 1) {
        target_level = current_level + 1;
    } else if (raw < RAW_LOW_THRESHOLD && current_level > 0) {
        target_level = current_level - 1;
    }

    /* 3. 如需切换，重配传感器并等待稳定 */
    if (target_level != current_level) {
        ESP_LOGI(TAG, "Level switch: %s -> %s (raw=%u)",
                 level_config[current_level].name,
                 level_config[target_level].name, raw);

        current_level = target_level;
        apply_level_config(current_level);
        vTaskDelay(pdMS_TO_TICKS(LEVEL_SWITCH_DELAY_MS));

        /* 用新配置重读 */
        if (veml7700_get_als_raw(&veml7700_device, &raw) != ESP_OK) {
            *als = 0;
            return;
        }
    }

    /* 4. 用现有驱动做 lux 换算 */
    if (veml7700_get_ambient_light(&veml7700_device, &veml7700_configuration, als) != ESP_OK) {
        *als = 0;
        return;
    }

    /* 5. 应用本级别幂律校准: cal = a * lux^b */
    veml7700_calib_t *cal = &calibration[current_level];
    float cal_lux = cal->a * powf((float)(*als), cal->b);
    if (cal_lux < 0.0f) cal_lux = 0.0f;
    *als = (uint32_t)(cal_lux + 0.5f);
}

void hw_veml7700_get_white_channel(uint32_t* white) {
    veml7700_get_white_channel(&veml7700_device, &veml7700_configuration, white);
}

void hw_veml7700_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down VEML7700");
    veml7700_configuration.shutdown = 1;
    veml7700_set_config(&veml7700_device, &veml7700_configuration);
}