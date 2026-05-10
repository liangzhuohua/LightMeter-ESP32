#include "hw_veml7700.h"
#include <string.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "hw_veml7700";

i2c_dev_t veml7700_device;
veml7700_config_t veml7700_configuration;

/* ── 自适应量程级别配置（遵循 Vishay AN 84323） ────────────────
 *
 * 官方分辨率基准: GAIN_2 + 800ms = 0.0042 lx/cnt
 * 其余组合按 ×2 / ÷2 法则由基准推导。
 *
 * 官方推荐流程（AN 第21页）:
 *   1. 从最低灵敏度起步: GAIN_1/8, IT=100ms
 *   2. 读 raw counts
 *   3. ≤100 counts → 升增益(1/8→1/4→2 with longer IT)
 *   4. 100 < counts ≤ 10000 → 当前增益可用，计算 lux
 *   5. >10000 counts (在 GAIN_1/8) → 缩短 IT (100→50→25ms)
 *
 * 各级别对应增益、积分时间和分辨率:
 *   L0: GAIN_2,    800ms — 0.0042 lx/cnt,  max ~275 lx (AN: 仅用于 <100 lx)
 *   L1: GAIN_1/4,  100ms — 0.2688 lx/cnt,  max ~17.6k lx
 *   L2: GAIN_1/8,  100ms — 0.5376 lx/cnt,  max ~35.2k lx (默认起步)
 *   L3: GAIN_1/8,   50ms — 1.0752 lx/cnt,  max ~70.5k lx
 *   L4: GAIN_1/8,   25ms — 2.1504 lx/cnt,  max ~141k lx
 */
static const struct {
    uint16_t gain;
    uint16_t integration_time;
    uint16_t it_ms;
    float    lux_per_count;
    const char *name;
} level_config[VEML7700_LEVEL_COUNT] = {
    [VEML7700_LEVEL_0] = {VEML7700_GAIN_2,     VEML7700_INTEGRATION_TIME_800MS, 800, 0.0042f,  "WEAK"},
    [VEML7700_LEVEL_1] = {VEML7700_GAIN_DIV_4, VEML7700_INTEGRATION_TIME_100MS, 100, 0.2688f, "MODERATE"},
    [VEML7700_LEVEL_2] = {VEML7700_GAIN_DIV_8, VEML7700_INTEGRATION_TIME_100MS, 100, 0.5376f, "BRIGHT"},
    [VEML7700_LEVEL_3] = {VEML7700_GAIN_DIV_8, VEML7700_INTEGRATION_TIME_50MS,   50, 1.0752f, "VBRIGHT"},
    [VEML7700_LEVEL_4] = {VEML7700_GAIN_DIV_8, VEML7700_INTEGRATION_TIME_25MS,   25, 2.1504f, "EXTREME"},
};

/* ── 换挡阈值（基于 raw counts，与官方流程图一致） ── */
#define COUNT_LOW     100     /* ≤100 counts → 太暗，提高灵敏度 */
#define COUNT_HIGH    10000   /* >10000 counts (GAIN_1/8) → 太亮，缩短IT */
#define RAW_SATURATION 55000  /* raw 饱和安全网 */

static veml7700_level_t current_level = VEML7700_LEVEL_2; /* 默认从最低灵敏度起步 */
static int switch_cooldown = 0;

/* ── AN 官方非线性校正公式（4次多项式） ────────────────
 *
 * Lux_corrected = a·x⁴ + b·x³ + c·x² + d·x
 * 其中 x = 未校正 lux 值 (raw × resolution)
 *
 * 来源: Vishay AN 84323 第10页 Fig. 9
 * 适用于 GAIN_1/4 和 GAIN_1/8，lux > 100 时非线性明显。
 * GAIN_1/2 (L0) 在 <100 lx 范围内线性，无需校正。
 * 多项式在 x ≤ 25000 范围内表现良好，超出则逐步发散——
 * auto-ranging 保证 counts ≤ 10000 即 x ≤ 21504 (L4: 10000×2.1504)，确保在有效区间内。
 */
static float an_lux_correction(float x)
{
    /* Vishay AN 84323 官方多项式系数 */
    const float a = 6.0135e-13f;
    const float b = -9.3924e-09f;
    const float c = 8.1488e-05f;
    const float d = 1.0023f;

    float x2 = x * x;
    float x3 = x2 * x;
    float x4 = x3 * x;
    return a * x4 + b * x3 + c * x2 + d * x;
}

/* 盖板玻璃透光率补偿系数（默认 1.0 = 无盖板/透明盖板）
 * 对于暗色盖板（如 10% 透光），设置为 10.0 (= 1/0.1) */
static float transmission_factor = 1.0f;

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

/* 设置盖板玻璃透光率补偿系数
 * 例如暗色盖板透光 10%，则 factor = 10.0 (= 1/0.1) */
void hw_veml7700_set_transmission(float factor)
{
    transmission_factor = factor;
    ESP_LOGI(TAG, "Transmission factor: %.4f", factor);
}

/* 应用指定量程级别的增益和积分时间配置到传感器 */
static void apply_level_config(veml7700_level_t level)
{
    veml7700_configuration.gain = level_config[level].gain;
    veml7700_configuration.integration_time = level_config[level].integration_time;
    veml7700_set_config(&veml7700_device, &veml7700_configuration);
    /* AN: 唤醒后需等待 ≥2.5ms 让信号处理器和振荡器稳定 */
    vTaskDelay(pdMS_TO_TICKS(3));
}

/* ── 初始化 / 关断 ──────────────────────────────── */

/* 比较两组VEML7700配置是否一致 */
static bool compare_configuration(veml7700_config_t *config_a, veml7700_config_t *config_b)
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
 * @brief 初始化VEML7700环境光传感器
 *
 * 遵循 AN 84323: 从最低灵敏度 (GAIN_1/8, IT=100ms) 起步避免饱和。
 * PSM 省电模式仅在 IT≥100ms 时有官方定义，故 L3/L4 不使用 PSM。
 *
 * @param power_saving_mode  省电模式 (VEML7700_POWER_SAVING_MODE_*)
 */
void hw_veml7700_init(uint16_t power_saving_mode)
{
    memset(&veml7700_device, 0, sizeof(i2c_dev_t));
    memset(&veml7700_configuration, 0, sizeof(veml7700_config_t));

    ESP_LOGI(TAG, "initializing hardware");

    ESP_ERROR_CHECK(veml7700_init_desc(&veml7700_device, HW_VEML7700_I2C_NUM, HW_VEML7700_SDA, HW_VEML7700_SCL));
    ESP_ERROR_CHECK(veml7700_probe(&veml7700_device));

    /* 从 LEVEL_2 (GAIN_1/8, IT=100ms) 起步 — 最低灵敏度，符合官方推荐 */
    current_level = VEML7700_LEVEL_2;
    veml7700_configuration.gain = level_config[current_level].gain;
    veml7700_configuration.integration_time = level_config[current_level].integration_time;

    veml7700_configuration.persistence_protect = VEML7700_PERSISTENCE_PROTECTION_4;
    veml7700_configuration.interrupt_enable = 1;
    veml7700_configuration.shutdown = 0;

    veml7700_configuration.power_saving_mode = power_saving_mode;
    veml7700_configuration.power_saving_enable = 1;

    ESP_ERROR_CHECK(veml7700_set_config(&veml7700_device, &veml7700_configuration));

    /* AN: 设置 shutdown=0 后需等待 ≥2.5ms */
    vTaskDelay(pdMS_TO_TICKS(3));

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
 * @brief 读取环境光lux值（自适应量程 + AN多项式校正 + 透光补偿）
 *
 * 遵循 AN 84323 流程图:
 *   1. 从 GAIN_1/8 + IT=100ms 起步
 *   2. raw ≤ 100  → 升增益 / 加长IT
 *   3. raw > 10000 (GAIN_1/8) → 缩短IT
 *   4. 未校正 lux → AN4次多项式校正 → 透光系数补偿 → 输出
 */
void hw_veml7700_get_ambient_light(uint32_t *als)
{
    uint16_t raw;
    float lux;
    bool switched;
    int max_iter = 5;

    do {
        switched = false;

        /* 1. 单次读取 raw count */
        if (veml7700_get_als_raw(&veml7700_device, &raw) != ESP_OK) {
            *als = 0;
            return;
        }

        /* 2. 用本次 raw 直接计算 lux（避免二次读 ALS 寄存器） */
        lux = (float)raw * level_config[current_level].lux_per_count;

        /* 3. 基于 raw counts 的换挡判断（与官方流程图一致） */
        if (switch_cooldown > 0) {
            switch_cooldown--;
        } else {
            switch (current_level) {

            /* ── L0: GAIN_2 + 800ms（最灵敏）── */
            case VEML7700_LEVEL_0:
                /* AN: gain 2 不应超过 100 lx（非线性），
                 * 100 lx at L0 = 23810 counts，取 20000 留余量 */
                if (raw > RAW_SATURATION || raw > 20000) {
                    ESP_LOGI(TAG, "L0->L1 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_1;
                    switched = true;
                }
                break;

            /* ── L1: GAIN_1/4 + 100ms ── */
            case VEML7700_LEVEL_1:
                if (raw > RAW_SATURATION) {
                    ESP_LOGI(TAG, "L1->L2 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_2;
                    switched = true;
                } else if (raw <= COUNT_LOW) {
                    ESP_LOGI(TAG, "L1->L0 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_0;
                    switched = true;
                }
                break;

            /* ── L2: GAIN_1/8 + 100ms（默认起步）── */
            case VEML7700_LEVEL_2:
                if (raw > COUNT_HIGH) {
                    ESP_LOGI(TAG, "L2->L3 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_3;
                    switched = true;
                } else if (raw <= COUNT_LOW) {
                    ESP_LOGI(TAG, "L2->L1 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_1;
                    switched = true;
                }
                break;

            /* ── L3: GAIN_1/8 + 50ms ── */
            case VEML7700_LEVEL_3:
                if (raw > COUNT_HIGH) {
                    ESP_LOGI(TAG, "L3->L4 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_4;
                    switched = true;
                } else if (raw <= COUNT_LOW) {
                    ESP_LOGI(TAG, "L3->L2 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_2;
                    switched = true;
                }
                break;

            /* ── L4: GAIN_1/8 + 25ms（最不灵敏）── */
            case VEML7700_LEVEL_4:
                if (raw <= COUNT_LOW) {
                    ESP_LOGI(TAG, "L4->L3 lux=%.0f raw=%u", lux, raw);
                    current_level = VEML7700_LEVEL_3;
                    switched = true;
                }
                break;

            default:
                break;
            }
        }

        /* 4. 如果换挡，重配传感器，设冷却期，等待新积分完成 */
        if (switched) {
            apply_level_config(current_level);
            switch_cooldown = 2;
            vTaskDelay(pdMS_TO_TICKS(level_config[current_level].it_ms + 50));
        }
    } while (switched && --max_iter > 0);

    /* 5. 应用 AN 官方非线性校正 + 盖板透光补偿
     *    GAIN_1/4 和 GAIN_1/8 级别使用 4 次多项式校正 (AN §10)
     *    GAIN_2 (L0) 在 <100 lx 范围线性，仅做透光补偿 */
    float corrected;
    if (current_level >= VEML7700_LEVEL_1) {
        corrected = an_lux_correction(lux);
    } else {
        corrected = lux;
    }
    corrected *= transmission_factor;
    if (corrected < 0.0f) corrected = 0.0f;
    if (corrected > 140000.0f) corrected = 140000.0f;
    *als = (uint32_t)(corrected + 0.5f);
}

/* 读取VEML7700白通道原始值 */
void hw_veml7700_get_white_channel(uint32_t *white)
{
    veml7700_get_white_channel(&veml7700_device, &veml7700_configuration, white);
}

/* 关闭VEML7700传感器（设置shutdown位） */
void hw_veml7700_shutdown(void)
{
    ESP_LOGI(TAG, "Shutting down VEML7700");
    veml7700_configuration.shutdown = 1;
    veml7700_set_config(&veml7700_device, &veml7700_configuration);
}
