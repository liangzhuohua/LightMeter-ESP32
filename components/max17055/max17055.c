#include "max17055.h"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define I2C_FREQ_HZ (400000)

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        esp_err_t __;                                                                                                  \
        if ((__ = x) != ESP_OK)                                                                                        \
            return __;                                                                                                 \
    }                                                                                                                  \
    while (0)
#define CHECK_ARG(VAL)                                                                                                 \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(VAL))                                                                                                    \
            return ESP_ERR_INVALID_ARG;                                                                                \
    }                                                                                                                  \
    while (0)

static const char *TAG = "max17055";

static float s_rsense_ohm = 0.01f;

static esp_err_t read_reg(i2c_dev_t *dev, uint8_t reg, uint16_t *data)
{
    CHECK_ARG(dev && data);
    I2C_DEV_CHECK(dev, i2c_dev_read(dev, &reg, 1, data, 2));
    return ESP_OK;
}

static esp_err_t write_reg(i2c_dev_t *dev, uint8_t reg, uint16_t data)
{
    CHECK_ARG(dev);
    I2C_DEV_CHECK(dev, i2c_dev_write(dev, &reg, 1, &data, 2));
    return ESP_OK;
}

static esp_err_t write_and_verify_reg(i2c_dev_t *dev, uint8_t reg, uint16_t data)
{
    CHECK_ARG(dev);
    int retries = 8;
    uint16_t read_data;

    do {
        CHECK(write_reg(dev, reg, data));
        vTaskDelay(pdMS_TO_TICKS(3));
        CHECK(read_reg(dev, reg, &read_data));
        if (read_data == data)
            return ESP_OK;
        retries--;
    } while (retries > 0);

    ESP_LOGE(TAG, "write_and_verify failed: reg=0x%02X, wrote=0x%04X, read=0x%04X", reg, data, read_data);
    return ESP_ERR_INVALID_RESPONSE;
}

static int16_t to_signed(uint16_t val)
{
    return (int16_t)val;
}

static esp_err_t poll_flag_clear(i2c_dev_t *dev, uint8_t reg, uint16_t mask, int timeout_ms)
{
    uint16_t data;
    int elapsed = 0;

    do {
        vTaskDelay(pdMS_TO_TICKS(10));
        CHECK(read_reg(dev, reg, &data));
        if (!(data & mask))
            return ESP_OK;
        elapsed += 10;
    } while (elapsed < timeout_ms);

    ESP_LOGE(TAG, "poll_flag_clear timeout: reg=0x%02X, mask=0x%04X, data=0x%04X", reg, mask, data);
    return ESP_ERR_TIMEOUT;
}

static esp_err_t forced_exit_hiber_mode(i2c_dev_t *dev, uint16_t *hibcfg_value)
{
    CHECK(read_reg(dev, MAX17055_REG_HIBCFG, hibcfg_value));
    CHECK(write_reg(dev, 0x60, 0x90));
    CHECK(write_reg(dev, MAX17055_REG_HIBCFG, 0x0000));
    CHECK(write_reg(dev, 0x60, 0x0000));
    return ESP_OK;
}

static esp_err_t ez_config_init(i2c_dev_t *dev, const max17055_config_t *cfg)
{
    const float charger_th = 4.275f;
    const int chg_v_high = 51200;
    const int chg_v_low = 44138;
    const int param_ez_fg1 = 0x8400;
    const int param_ez_fg2 = 0x8000;

    uint16_t design_cap = cfg->design_cap;
    uint16_t dqacc = design_cap >> 5;
    uint16_t dpacc;

    CHECK(write_reg(dev, MAX17055_REG_DESIGNCAP, design_cap));
    CHECK(write_reg(dev, MAX17055_REG_DQACC, dqacc));
    CHECK(write_reg(dev, MAX17055_REG_ICHGTERM, cfg->ichg_term));
    CHECK(write_reg(dev, MAX17055_REG_VEMPTY, cfg->vempty));

    if (cfg->vcharge > charger_th) {
        dpacc = (uint16_t)((dqacc * chg_v_high) / design_cap);
        CHECK(write_reg(dev, MAX17055_REG_DPACC, dpacc));
        CHECK(write_reg(dev, MAX17055_REG_MODELCFG, param_ez_fg1));
    } else {
        dpacc = (uint16_t)((dqacc * chg_v_low) / design_cap);
        CHECK(write_reg(dev, MAX17055_REG_DPACC, dpacc));
        CHECK(write_reg(dev, MAX17055_REG_MODELCFG, param_ez_fg2));
    }

    return ESP_OK;
}

esp_err_t max17055_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    CHECK_ARG(dev);

    dev->port = port;
    dev->addr = MAX17055_I2C_ADDR;
    dev->cfg.sda_io_num = sda_gpio;
    dev->cfg.scl_io_num = scl_gpio;
#if HELPER_TARGET_IS_ESP32
    dev->cfg.master.clk_speed = I2C_FREQ_HZ;
#endif

    return i2c_dev_create_mutex(dev);
}

esp_err_t max17055_free_desc(i2c_dev_t *dev)
{
    CHECK_ARG(dev);
    return i2c_dev_delete_mutex(dev);
}

esp_err_t max17055_probe(i2c_dev_t *dev)
{
    CHECK_ARG(dev);

    I2C_DEV_TAKE_MUTEX(dev);
    esp_err_t err = i2c_dev_probe(dev, I2C_DEV_WRITE);
    I2C_DEV_GIVE_MUTEX(dev);
    return err;
}

esp_err_t max17055_init(i2c_dev_t *dev, const max17055_config_t *cfg)
{
    CHECK_ARG(dev && cfg);

    s_rsense_ohm = cfg->rsense_mohm / 1000.0f;

    uint16_t status_val;
    uint16_t hibcfg_value;

    I2C_DEV_TAKE_MUTEX(dev);

    CHECK(read_reg(dev, MAX17055_REG_STATUS, &status_val));
    if (!(status_val & MAX17055_STATUS_POR_MASK)) {
        ESP_LOGW(TAG, "POR bit not set, skipping full init");
        I2C_DEV_GIVE_MUTEX(dev);
        return ESP_OK;
    }

    CHECK(poll_flag_clear(dev, MAX17055_REG_FSTAT, MAX17055_FSTAT_DNR_MASK, 1000));

    CHECK(forced_exit_hiber_mode(dev, &hibcfg_value));

    CHECK(ez_config_init(dev, cfg));

    CHECK(poll_flag_clear(dev, MAX17055_REG_MODELCFG, MAX17055_MODELCFG_REFRESH_MASK, 500));

    CHECK(write_reg(dev, MAX17055_REG_HIBCFG, hibcfg_value));

    CHECK(read_reg(dev, MAX17055_REG_STATUS, &status_val));
    CHECK(write_and_verify_reg(dev, MAX17055_REG_STATUS, status_val & MAX17055_POR_CLEAR_MASK));

    I2C_DEV_GIVE_MUTEX(dev);

    ESP_LOGI(TAG, "MAX17055 EZ config init done");
    return ESP_OK;
}

esp_err_t max17055_get_vcell(i2c_dev_t *dev, float *voltage_mv)
{
    CHECK_ARG(dev && voltage_mv);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_VCELL, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    *voltage_mv = (float)raw * MAX17055_VCELL_LSB_UV / 1000.0f;
    return ESP_OK;
}

esp_err_t max17055_get_avg_vcell(i2c_dev_t *dev, float *voltage_mv)
{
    CHECK_ARG(dev && voltage_mv);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_AVGVCELL, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    *voltage_mv = (float)raw * MAX17055_VCELL_LSB_UV / 1000.0f;
    return ESP_OK;
}

esp_err_t max17055_get_current(i2c_dev_t *dev, float *current_ma)
{
    CHECK_ARG(dev && current_ma);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_CURRENT, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    int16_t signed_val = to_signed(raw);
    float current_lsb_ua = MAX17055_CURRENT_LSB_UV / s_rsense_ohm;
    *current_ma = (float)signed_val * current_lsb_ua / 1000.0f;
    return ESP_OK;
}

esp_err_t max17055_get_avg_current(i2c_dev_t *dev, float *current_ma)
{
    CHECK_ARG(dev && current_ma);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_AVGCURRENT, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    int16_t signed_val = to_signed(raw);
    float current_lsb_ua = MAX17055_CURRENT_LSB_UV / s_rsense_ohm;
    *current_ma = (float)signed_val * current_lsb_ua / 1000.0f;
    return ESP_OK;
}

esp_err_t max17055_get_soc(i2c_dev_t *dev, float *soc_pct)
{
    CHECK_ARG(dev && soc_pct);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_REPSOC, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    *soc_pct = (float)raw / 256.0f;
    return ESP_OK;
}

esp_err_t max17055_get_rep_cap(i2c_dev_t *dev, float *cap_mah)
{
    CHECK_ARG(dev && cap_mah);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_REPCAP, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    float cap_lsb_mah = MAX17055_CAPACITY_LSB_UVH / s_rsense_ohm / 1000.0f;
    *cap_mah = (float)raw * cap_lsb_mah;
    return ESP_OK;
}

esp_err_t max17055_get_temperature(i2c_dev_t *dev, float *temp_c)
{
    CHECK_ARG(dev && temp_c);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_TEMP, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    int16_t signed_val = to_signed(raw);
    *temp_c = (float)(signed_val >> 8);
    return ESP_OK;
}

esp_err_t max17055_get_tte(i2c_dev_t *dev, float *tte_s)
{
    CHECK_ARG(dev && tte_s);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_TTE, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    if (raw == 0xFFFF) {
        *tte_s = -1.0f;
    } else {
        *tte_s = (float)raw * MAX17055_TTE_TTF_LSB_S;
    }
    return ESP_OK;
}

esp_err_t max17055_get_ttf(i2c_dev_t *dev, float *ttf_s)
{
    CHECK_ARG(dev && ttf_s);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_TTF, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    if (raw == 0xFFFF) {
        *ttf_s = -1.0f;
    } else {
        *ttf_s = (float)raw * MAX17055_TTE_TTF_LSB_S;
    }
    return ESP_OK;
}

esp_err_t max17055_get_cycles(i2c_dev_t *dev, uint16_t *cycles)
{
    CHECK_ARG(dev && cycles);

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_CYCLES, cycles));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

esp_err_t max17055_get_age(i2c_dev_t *dev, uint8_t *age_pct)
{
    CHECK_ARG(dev && age_pct);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_AGE, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    *age_pct = (uint8_t)(raw >> 8);
    return ESP_OK;
}

esp_err_t max17055_get_full_cap(i2c_dev_t *dev, float *cap_mah)
{
    CHECK_ARG(dev && cap_mah);

    uint16_t raw;
    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_FULLCAPREP, &raw));
    I2C_DEV_GIVE_MUTEX(dev);

    float cap_lsb_mah = MAX17055_CAPACITY_LSB_UVH / s_rsense_ohm / 1000.0f;
    *cap_mah = (float)raw * cap_lsb_mah;
    return ESP_OK;
}

esp_err_t max17055_get_status(i2c_dev_t *dev, uint16_t *status)
{
    CHECK_ARG(dev && status);

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, read_reg(dev, MAX17055_REG_STATUS, status));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

esp_err_t max17055_set_valrt(i2c_dev_t *dev, float v_min_mv, float v_max_mv)
{
    CHECK_ARG(dev);

    uint8_t v_min = (uint8_t)(v_min_mv / 20.0f);
    uint8_t v_max = (uint8_t)(v_max_mv / 20.0f);
    uint16_t valrt = ((uint16_t)v_max << 8) | v_min;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, write_reg(dev, MAX17055_REG_VALRTTH, valrt));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

esp_err_t max17055_set_salrt(i2c_dev_t *dev, uint8_t soc_min, uint8_t soc_max)
{
    CHECK_ARG(dev);

    uint16_t salrt = ((uint16_t)soc_max << 8) | soc_min;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, write_reg(dev, MAX17055_REG_SALRTTH, salrt));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}

esp_err_t max17055_set_ialrt(i2c_dev_t *dev, float i_min_ma, float i_max_ma)
{
    CHECK_ARG(dev);

    float current_lsb_ua = MAX17055_CURRENT_LSB_UV / s_rsense_ohm;
    uint8_t i_min = (uint8_t)(i_min_ma * 1000.0f / current_lsb_ua / 400.0f);
    uint8_t i_max = (uint8_t)(i_max_ma * 1000.0f / current_lsb_ua / 400.0f);
    uint16_t ialrt = ((uint16_t)i_max << 8) | i_min;

    I2C_DEV_TAKE_MUTEX(dev);
    I2C_DEV_CHECK(dev, write_reg(dev, MAX17055_REG_IALRTTH, ialrt));
    I2C_DEV_GIVE_MUTEX(dev);

    return ESP_OK;
}
