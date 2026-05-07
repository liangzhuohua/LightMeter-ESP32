#include "hw_max17055.h"
#include <string.h>
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include "i2cdev.h"
#include "max17055.h"

static const char *TAG = "hw_max17055";

static i2c_dev_t max17055_device;

/* 初始化MAX17055的ALRT引脚为输入模式 */
static void alrt_gpio_init(void)
{
    rtc_gpio_deinit(HW_MAX17055_ALRT_GPIO);

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HW_MAX17055_ALRT_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
}

/**
 * @brief 初始化MAX17055电量计：配置电池参数，读取初始SOC/电压
 */
void hw_max17055_init(void)
{
    memset(&max17055_device, 0, sizeof(i2c_dev_t));

    ESP_LOGI(TAG, "initializing MAX17055 fuel gauge");

    ESP_ERROR_CHECK(max17055_init_desc(&max17055_device, HW_MAX17055_I2C_NUM, HW_MAX17055_SDA, HW_MAX17055_SCL));

    ESP_ERROR_CHECK(max17055_probe(&max17055_device));

    float rsense_ohm = HW_MAX17055_RSENSE_MOHM / 1000.0f;
    float cap_lsb_mah = MAX17055_CAPACITY_LSB_UVH / rsense_ohm / 1000.0f;
    float current_lsb_ma = MAX17055_CURRENT_LSB_UV / rsense_ohm / 1000.0f;

    uint16_t design_cap = (uint16_t)(HW_MAX17055_BATT_CAP_MAH / cap_lsb_mah);
    uint16_t ichg_term = (uint16_t)(30.0f / current_lsb_ma);

    uint16_t ve_reg = (uint16_t)(HW_MAX17055_VEMPTY * 100.0f);
    uint16_t vr_reg = (uint16_t)(HW_MAX17055_VRECOVERY * 25.0f);
    uint16_t vempty = (ve_reg << 7) | (vr_reg & 0x7F);

    max17055_config_t cfg = {
        .design_cap = design_cap,
        .ichg_term = ichg_term,
        .vempty = vempty,
        .vcharge = HW_MAX17055_VCHARGE,
        .rsense_mohm = HW_MAX17055_RSENSE_MOHM,
    };

    ESP_LOGI(TAG, "design_cap=0x%04X (%u), ichg_term=0x%04X (%u), vempty=0x%04X, vcharge=%.1fV, rsense=%.1fmOhm",
             design_cap, design_cap, ichg_term, ichg_term, vempty, cfg.vcharge, cfg.rsense_mohm);

    ESP_ERROR_CHECK(max17055_init(&max17055_device, &cfg));

    ESP_ERROR_CHECK(max17055_set_valrt(&max17055_device, 3000.0f, 4200.0f));
    ESP_ERROR_CHECK(max17055_set_salrt(&max17055_device, 1, 99));

    alrt_gpio_init();

    float vcell, soc, temp;
    max17055_get_vcell(&max17055_device, &vcell);
    max17055_get_soc(&max17055_device, &soc);
    max17055_get_temperature(&max17055_device, &temp);
    ESP_LOGI(TAG, "initial read: Vcell=%.2fmV, SOC=%.1f%%, Temp=%.1fC", vcell, soc, temp);
}

/* 获取电池电压(mV) */
void hw_max17055_get_vcell(float *voltage_mv)
{
    max17055_get_vcell(&max17055_device, voltage_mv);
}

/* 获取平均电池电压(mV) */
void hw_max17055_get_avg_vcell(float *voltage_mv)
{
    max17055_get_avg_vcell(&max17055_device, voltage_mv);
}

/* 获取瞬时电流(mA) */
void hw_max17055_get_current(float *current_ma)
{
    max17055_get_current(&max17055_device, current_ma);
}

/* 获取平均电流(mA) */
void hw_max17055_get_avg_current(float *current_ma)
{
    max17055_get_avg_current(&max17055_device, current_ma);
}

/* 获取电量百分比(SOC, %) */
void hw_max17055_get_soc(float *soc_pct)
{
    max17055_get_soc(&max17055_device, soc_pct);
}

/* 获取报告容量(mAh) */
void hw_max17055_get_rep_cap(float *cap_mah)
{
    max17055_get_rep_cap(&max17055_device, cap_mah);
}

/* 获取电池温度(°C) */
void hw_max17055_get_temperature(float *temp_c)
{
    max17055_get_temperature(&max17055_device, temp_c);
}

/* 获取放空时间估算(TTE, 秒) */
void hw_max17055_get_tte(float *tte_s)
{
    max17055_get_tte(&max17055_device, tte_s);
}

/* 获取充满时间估算(TTF, 秒) */
void hw_max17055_get_ttf(float *ttf_s)
{
    max17055_get_ttf(&max17055_device, ttf_s);
}

/* 获取满充容量(mAh) */
void hw_max17055_get_full_cap(float *cap_mah)
{
    max17055_get_full_cap(&max17055_device, cap_mah);
}

/* 获取充放电循环次数 */
void hw_max17055_get_cycles(uint16_t *cycles)
{
    max17055_get_cycles(&max17055_device, cycles);
}

/* 获取电池老化百分比 */
void hw_max17055_get_age(uint8_t *age_pct)
{
    max17055_get_age(&max17055_device, age_pct);
}

/* 通过电流方向判断是否正在充电 */
bool hw_max17055_is_charging(void)
{
    float current = 0.0f;
    if (max17055_get_current(&max17055_device, &current) != ESP_OK)
        return false;
    return (current > 1.0f);
}

/* 获取MAX17055状态寄存器值 */
void hw_max17055_get_status(uint16_t *status)
{
    max17055_get_status(&max17055_device, status);
}

/* 将MAX17055置于休眠模式以节省功耗 */
void hw_max17055_sleep(void)
{
    ESP_LOGI(TAG, "Putting MAX17055 into hibernate mode");
    uint16_t hibcfg;
    i2c_dev_read_reg(&max17055_device, MAX17055_REG_HIBCFG, &hibcfg, 2);
    hibcfg |= (1 << 0);
    i2c_dev_write_reg(&max17055_device, MAX17055_REG_HIBCFG, &hibcfg, 2);
}

/* 释放ALRT引脚（为深度睡眠做准备，防止漏电） */
void hw_max17055_release_pins(void)
{
    ESP_LOGI(TAG, "Releasing ALRT pin for deep sleep");
    rtc_gpio_init(HW_MAX17055_ALRT_GPIO);
    rtc_gpio_set_direction(HW_MAX17055_ALRT_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(HW_MAX17055_ALRT_GPIO);
    rtc_gpio_pullup_dis(HW_MAX17055_ALRT_GPIO);
}

/**
 * @brief 强制校准满充状态：将REPSOC设为100%，更新满充容量
 */
void hw_max17055_force_full(float full_capacity_mah)
{
    ESP_LOGI(TAG, "Force full charge calibration");

    // 读取当前SOC和电压
    float soc, voltage;
    max17055_get_soc(&max17055_device, &soc);
    max17055_get_vcell(&max17055_device, &voltage);
    ESP_LOGI(TAG, "Before calibration: SOC=%.1f%%, Vcell=%.2fmV", soc, voltage);

    // 设置满充状态：将REPSOC设置为100%
    // 同时更新REPCAP以匹配满充容量
    float rsense_ohm = HW_MAX17055_RSENSE_MOHM / 1000.0f;
    float cap_lsb_mah = MAX17055_CAPACITY_LSB_UVH / rsense_ohm / 1000.0f;

    uint16_t full_cap_reg;
    if (full_capacity_mah > 0) {
        full_cap_reg = (uint16_t)(full_capacity_mah / cap_lsb_mah);
    } else {
        // 读取当前学习的满充容量
        max17055_get_full_cap(&max17055_device, &full_capacity_mah);
        full_cap_reg = (uint16_t)(full_capacity_mah / cap_lsb_mah);
    }

    // 写入REPSOC = 100% (256 * 100% = 0x6400)
    i2c_dev_write_reg(&max17055_device, MAX17055_REG_REPSOC, &(uint16_t){0x6400}, 2);

    // 更新REPCAP以匹配满充容量
    i2c_dev_write_reg(&max17055_device, MAX17055_REG_REPCAP, &full_cap_reg, 2);

    // 验证结果
    max17055_get_soc(&max17055_device, &soc);
    max17055_get_vcell(&max17055_device, &voltage);
    ESP_LOGI(TAG, "After calibration: SOC=%.1f%%, Vcell=%.2fmV", soc, voltage);
}
