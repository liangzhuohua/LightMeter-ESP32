#include "hw_veml7700.h"
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hw_veml7700";

i2c_dev_t veml7700_device;
veml7700_config_t veml7700_configuration;

/* ── 自适应量程（lux为主 raw为辅 + 趋势确认）────── */

/* 各级别对应的增益、积分时间和分辨率
 *   弱光档: GAIN_2, 800ms — 0.0036 lx/bit, 最大 236 lx
 *   常规档: GAIN_1, 100ms — 0.0576 lx/bit, 最大 3775 lx
 *   强光档: GAIN_DIV_8, 25ms — 1.8432 lx/bit, 最大 120800 lx
 */
static const struct {
    uint16_t gain;
    uint16_t integration_time;
    uint16_t it_ms;
    float lux_per_count;      /* 分辨率 (lx/bit) */
    const char *name;
} level_config[VEML7700_LEVEL_COUNT] = {
    [VEML7700_LEVEL_0] = {VEML7700_GAIN_2,     VEML7700_INTEGRATION_TIME_800MS, 800, 0.0036f,  "WEAK"},
    [VEML7700_LEVEL_1] = {VEML7700_GAIN_1,     VEML7700_INTEGRATION_TIME_100MS, 100, 0.0576f,  "NORMAL"},
    [VEML7700_LEVEL_2] = {VEML7700_GAIN_DIV_8, VEML7700_INTEGRATION_TIME_25MS,   25, 1.8432f, "STRONG"},
};

/* ── 切换阈值 ──
 * 每个档位的推荐 lux 工作区间（与 raw 阈值不同，lux 跨档位可比）
 *   LEVEL_0:   0 ~ 200 lx   (max 236)
 *   LEVEL_1:  80 ~ 3200 lx  (max 3775)
 *   LEVEL_2: 2500 ~ 100000 lx (max 120800)
 * 滞回间隙: L0↔L1 的 80~200, L1↔L2 的 2500~3200
 */
#define L0_LUX_CEILING   200.0f
#define L1_LUX_FLOOR     80.0f
#define L1_LUX_CEILING   1200.0f  /* 照度计约1200 lx时切到LEVEL_2更准 */
#define L2_LUX_FLOOR     800.0f   /* 滞回间隙 800~1200 */

/* raw 饱和安全网: 任何档位超过此值立即升档 */
#define RAW_SATURATION   55000

static veml7700_level_t current_level = VEML7700_LEVEL_2;
static int switch_cooldown = 0;  /* 换挡冷却计数，防震荡 */

/* 逐级校准系数: calibrated = a * raw_lux^b */
static veml7700_calib_t calibration[VEML7700_LEVEL_COUNT] = {
    [VEML7700_LEVEL_0] = {0.8122f, 1.0534f},  /* R²=0.9996 */
    [VEML7700_LEVEL_1] = {0.5082f, 1.1443f},  /* R²=0.9893 */
    [VEML7700_LEVEL_2] = {0.1437f, 1.2420f},  /* R²=0.9812 */
};

/* 获取当前自适应量程级别 */
veml7700_level_t hw_veml7700_get_level(void)
{
    return current_level;
}

/* 获取当前量程级别对应的积分时间(ms) */
uint16_t hw_veml7700_get_it_ms(void)
{
    return level_config[current_level].it_ms;
}

/* 设置指定量程级别的校准系数（幂律模型：calibrated = a * raw^b） */
void hw_veml7700_set_calibration(veml7700_level_t level, float a, float b)
{
    if (level < VEML7700_LEVEL_COUNT) {
        calibration[level].a = a;
        calibration[level].b = b;
        ESP_LOGI(TAG, "Calib L%d: a=%.4f b=%.4f", level, a, b);
    }
}

/* 应用指定量程级别的增益和积分时间配置到传感器 */
static void apply_level_config(veml7700_level_t level)
{
    veml7700_configuration.gain = level_config[level].gain;
    veml7700_configuration.integration_time = level_config[level].integration_time;
    veml7700_set_config(&veml7700_device, &veml7700_configuration);
}

/* ── 初始化 / 关断 ──────────────────────────────── */

/* 比较两组VEML7700配置是否一致（用于验证写入是否成功） */
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

/**
 * @brief 初始化VEML7700环境光传感器（从强光档Level 2开始避免饱和）
 */
void hw_veml7700_init(uint16_t gain, uint16_t integration_time, uint16_t power_saving_mode) {

    memset(&veml7700_device, 0, sizeof(i2c_dev_t));
    memset(&veml7700_configuration, 0, sizeof(veml7700_config_t));

    ESP_LOGI(TAG, "initializing hardware");

    ESP_ERROR_CHECK(veml7700_init_desc(&veml7700_device, HW_VEML7700_I2C_NUM, HW_VEML7700_SDA, HW_VEML7700_SCL));

    ESP_ERROR_CHECK(veml7700_probe(&veml7700_device));

    /* 用 Level 2 (最低灵敏度) 作为初始配置，避免上电饱和 */
    current_level = VEML7700_LEVEL_2;
    veml7700_configuration.gain = level_config[current_level].gain;                    // 设置增益（GAIN_DIV_8 = 1/8x）
    veml7700_configuration.integration_time = level_config[current_level].integration_time; // 设置积分时间（25ms）

    veml7700_configuration.persistence_protect = VEML7700_PERSISTENCE_PROTECTION_4;   // 连续4次超限才触发中断，防抖动
    veml7700_configuration.interrupt_enable = 1;                                       // 启用中断功能
    veml7700_configuration.shutdown = 0;                                               // 0 = 正常工作模式（非关断）

    veml7700_configuration.power_saving_mode = power_saving_mode;                      // 省电模式（由外部传入）
    veml7700_configuration.power_saving_enable = 1;                                    // 启用省电模式

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

/**
 * @brief 读取环境光lux值（带自适应量程切换和幂律校准）
 * @note 自动在三级量程(WEAK/NORMAL/STRONG)之间切换，内置冷却期防震荡
 */
void hw_veml7700_get_ambient_light(uint32_t* als) {
    uint16_t raw;
    float lux;
    bool switched;
    int max_iter = 3;

    do {
        switched = false;

        /* 1. 读当前量程下的 raw count */
        if (veml7700_get_als_raw(&veml7700_device, &raw) != ESP_OK) {
            *als = 0;
            return;
        }

        /* 2. 换算 lux */
        lux = (float)raw * level_config[current_level].lux_per_count;

        /* 3. lux + raw 判断换挡（冷却期内禁止换挡） */
        if (switch_cooldown > 0) {
            switch_cooldown--;
        } else {
            if (current_level == VEML7700_LEVEL_0) {
                if (raw > RAW_SATURATION || lux > L0_LUX_CEILING) {
                    ESP_LOGI(TAG, "L0->L1 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_1;
                    switched = true;
                }
            } else if (current_level == VEML7700_LEVEL_1) {
                if (raw > RAW_SATURATION || lux > L1_LUX_CEILING) {
                    ESP_LOGI(TAG, "L1->L2 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_2;
                    switched = true;
                } else if (lux < L1_LUX_FLOOR) {
                    ESP_LOGI(TAG, "L1->L0 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_0;
                    switched = true;
                }
            } else { /* LEVEL_2 */
                if (lux < L2_LUX_FLOOR) {
                    ESP_LOGI(TAG, "L2->L1 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_1;
                    switched = true;
                }
            }
        }

        /* 4. 如切换，重配传感器，设冷却期，等待新积分时间 */
        if (switched) {
            apply_level_config(current_level);
            switch_cooldown = 2;  /* 换挡后冷却2次读数 */
            vTaskDelay(pdMS_TO_TICKS(level_config[current_level].it_ms + 50));
        }
    } while (switched && --max_iter > 0);

    /* 5. 用现有驱动做 lux 换算（内部自己读raw，数据来自当前量程） */
    if (veml7700_get_ambient_light(&veml7700_device, &veml7700_configuration, als) != ESP_OK) {
        *als = 0;
        return;
    }

    /* 6. 应用本级别幂律校准: cal = a * lux^b */
    veml7700_calib_t *cal = &calibration[current_level];
    float cal_lux = cal->a * powf((float)(*als), cal->b);
    if (cal_lux < 0.0f) cal_lux = 0.0f;
    *als = (uint32_t)(cal_lux + 0.5f);
}

/* 读取VEML7700白通道原始值 */
void hw_veml7700_get_white_channel(uint32_t* white) {
    veml7700_get_white_channel(&veml7700_device, &veml7700_configuration, white);
}

/* 关闭VEML7700传感器（设置shutdown位） */
void hw_veml7700_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down VEML7700");
    veml7700_configuration.shutdown = 1;
    veml7700_set_config(&veml7700_device, &veml7700_configuration);
}