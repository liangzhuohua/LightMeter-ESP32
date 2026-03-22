/*
 * SPDX-License-Identifier: ISC
 *
 * Copyright (c) 2022 Marc Luehr <marcluehr@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define VEML7700_COMMAND_CODE_ALS_CONF_0   (0)
#define VEML7700_COMMAND_CODE_ALS_WH       (1)
#define VEML7700_COMMAND_CODE_ALS_WL       (2)
#define VEML7700_COMMAND_CODE_POWER_SAVING (3)
#define VEML7700_COMMAND_CODE_ALS          (4)
#define VEML7700_COMMAND_CODE_WHITE        (5)
#define VEML7700_COMMAND_CODE_ALS_INT      (6)

#define VEML7700_GAIN_MASK  (0x1800)
#define VEML7700_GAIN_SHIFT (11)

#define VEML7700_INTEGRATION_TIME_MASK  (0x03C0)
#define VEML7700_INTEGRATION_TIME_SHIFT (6)

#define VEML7700_PERSISTENCE_PROTECTION_MASK  (0x0030)
#define VEML7700_PERSISTENCE_PROTECTION_SHIFT (4)

#define VEML7700_INTERRUPT_ENABLE_MASK  (0x0002)
#define VEML7700_INTERRUPT_ENABLE_SHIFT (1)

#define VEML7700_SHUTDOWN_MASK  (0x0001)
#define VEML7700_SHUTDOWN_SHIFT (0)

#define VEML7700_POWER_SAVING_MODE_MASK  (0x0060)
#define VEML7700_POWER_SAVING_MODE_SHIFT (1)

#define VEML7700_POWER_SAVING_MODE_ENABLE_MASK  (0x0001)
#define VEML7700_POWER_SAVING_MODE_ENABLE_SHIFT (0)

#define VEML7700_INTERRUPT_STATUS_LOW_MASK   (0x8000)
#define VEML7700_INTERRUPT_STATUS_LOW_SHIFT  (15)
#define VEML7700_INTERRUPT_STATUS_HIGH_MASK  (0x4000)
#define VEML7700_INTERRUPT_STATUS_HIGH_SHIFT (14)

#define VEML7700_RESOLUTION_800MS_IT_GAIN_2     (36)
#define VEML7700_RESOLUTION_800MS_IT_GAIN_2_DIV (10000)

/**
 * @file veml7700.c
 *
 * ESP-IDF driver for VEML7700 brightness sensors for I2C-bus (updated for native I2C)
 *
 * Copyright (c) 2022 Marc Luehr <marcluehr@gmail.com>
 *
 * MIT Licensed as described in the file LICENSE
 */

#include "veml7700.h"
#include "esp_log.h"

#define I2C_FREQ_HZ (200000)
#define I2C_TIMEOUT_MS (1000)

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

static esp_err_t read_port(i2c_dev_t *dev, uint8_t command_code, uint16_t *data)
{
    CHECK_ARG(dev);
    CHECK_ARG(data);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    CHECK(i2c_master_start(cmd));
    CHECK(i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true));
    CHECK(i2c_master_write_byte(cmd, command_code, true));
    CHECK(i2c_master_start(cmd));
    CHECK(i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_READ, true));

    uint8_t buf[2];
    CHECK(i2c_master_read(cmd, buf, 2, I2C_MASTER_LAST_NACK));
    CHECK(i2c_master_stop(cmd));

    esp_err_t ret = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        *data = buf[0] | (buf[1] << 8);  // Little-endian
    }
    return ret;
}

static esp_err_t write_port(i2c_dev_t *dev, uint8_t command_code, uint16_t data)
{
    CHECK_ARG(dev);

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    CHECK(i2c_master_start(cmd));
    CHECK(i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true));
    CHECK(i2c_master_write_byte(cmd, command_code, true));

    uint8_t data_low = data & 0xFF;
    uint8_t data_high = (data >> 8) & 0xFF;
    CHECK(i2c_master_write_byte(cmd, data_low, true));
    CHECK(i2c_master_write_byte(cmd, data_high, true));
    CHECK(i2c_master_stop(cmd));

    esp_err_t ret = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    i2c_cmd_link_delete(cmd);
    return ret;
}

static uint32_t resolution(veml7700_config_t *config)
{
    CHECK_ARG(config);

    uint32_t res = VEML7700_RESOLUTION_800MS_IT_GAIN_2;
    switch (config->gain)
    {
        case VEML7700_GAIN_1:
            res *= 2;
            break;
        case VEML7700_GAIN_DIV_4:
            res *= 8;
            break;
        case VEML7700_GAIN_DIV_8:
            res *= 16;
            break;
    }
    switch (config->integration_time)
    {
        case VEML7700_INTEGRATION_TIME_400MS:
            res *= 2;
            break;
        case VEML7700_INTEGRATION_TIME_200MS:
            res *= 4;
            break;
        case VEML7700_INTEGRATION_TIME_100MS:
            res *= 8;
            break;
        case VEML7700_INTEGRATION_TIME_50MS:
            res *= 16;
            break;
        case VEML7700_INTEGRATION_TIME_25MS:
            res *= 32;
            break;
    }
    return res;
}

///////////////////////////////////////////////////////////////////////////////

esp_err_t veml7700_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio)
{
    CHECK_ARG(dev);

    dev->port = port;
    dev->addr = VEML7700_I2C_ADDR;

    // 移除：不再初始化总线（由 bsp_i2c_init 负责，避免重复安装）
    // 注意：调用前必须确保 i2c_init() 已执行

    ESP_LOGI("VEML7700", "Device descriptor initialized (bus assumed ready)");
    return ESP_OK;
}

esp_err_t veml7700_free_desc(i2c_dev_t *dev)
{
    CHECK_ARG(dev);

    // return i2c_driver_delete(dev->port);
    return ESP_OK;
}

esp_err_t veml7700_probe(i2c_dev_t *dev)
{
    CHECK_ARG(dev);

    /* Use write request since read causes timeout; chip waits for command code */
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    CHECK(i2c_master_start(cmd));
    CHECK(i2c_master_write_byte(cmd, (dev->addr << 1) | I2C_MASTER_WRITE, true));
    CHECK(i2c_master_stop(cmd));

    esp_err_t err = i2c_master_cmd_begin(dev->port, cmd, pdMS_TO_TICKS(100));
    i2c_cmd_link_delete(cmd);
    if (err == ESP_OK) {
        ESP_LOGI("VEML7700", "Probe OK");
    } else {
        ESP_LOGE("VEML7700", "Probe FAIL: %s", esp_err_to_name(err));
    }
    return err;
}

esp_err_t veml7700_set_config(i2c_dev_t *dev, veml7700_config_t *config)
{
    CHECK_ARG(dev);
    CHECK_ARG(config);

    uint16_t config_data = 0;
    config_data |= ((uint16_t)config->gain) << VEML7700_GAIN_SHIFT;
    config_data |= ((uint16_t)config->integration_time) << VEML7700_INTEGRATION_TIME_SHIFT;
    config_data |= ((uint16_t)config->persistence_protect) << VEML7700_PERSISTENCE_PROTECTION_SHIFT;
    config_data |= ((uint16_t)config->interrupt_enable) << VEML7700_INTERRUPT_ENABLE_SHIFT;
    config_data |= ((uint16_t)config->shutdown) << VEML7700_SHUTDOWN_SHIFT;

    uint16_t power_saving_data = 0;
    power_saving_data |= ((uint16_t)config->power_saving_mode) << VEML7700_POWER_SAVING_MODE_SHIFT;
    power_saving_data |= ((uint16_t)config->power_saving_enable) << VEML7700_POWER_SAVING_MODE_ENABLE_SHIFT;

    CHECK(write_port(dev, VEML7700_COMMAND_CODE_ALS_CONF_0, config_data));
    CHECK(write_port(dev, VEML7700_COMMAND_CODE_ALS_WH, config->threshold_high));
    CHECK(write_port(dev, VEML7700_COMMAND_CODE_ALS_WL, config->threshold_low));
    CHECK(write_port(dev, VEML7700_COMMAND_CODE_POWER_SAVING, power_saving_data));
    return ESP_OK;
}

esp_err_t veml7700_get_config(i2c_dev_t *dev, veml7700_config_t *config)
{
    CHECK_ARG(dev);
    CHECK_ARG(config);

    uint16_t config_data = 0;
    uint16_t power_saving_data = 0;

    CHECK(read_port(dev, VEML7700_COMMAND_CODE_ALS_CONF_0, &config_data));
    CHECK(read_port(dev, VEML7700_COMMAND_CODE_ALS_WH, &(config->threshold_high)));
    CHECK(read_port(dev, VEML7700_COMMAND_CODE_ALS_WL, &(config->threshold_low)));
    CHECK(read_port(dev, VEML7700_COMMAND_CODE_POWER_SAVING, &power_saving_data));

    config->gain = (config_data & VEML7700_GAIN_MASK) >> VEML7700_GAIN_SHIFT;
    config->integration_time = (config_data & VEML7700_INTEGRATION_TIME_MASK) >> VEML7700_INTEGRATION_TIME_SHIFT;
    config->persistence_protect = (config_data & VEML7700_PERSISTENCE_PROTECTION_MASK) >> VEML7700_PERSISTENCE_PROTECTION_SHIFT;
    config->interrupt_enable = (config_data & VEML7700_INTERRUPT_ENABLE_MASK) >> VEML7700_INTERRUPT_ENABLE_SHIFT;
    config->shutdown = (config_data & VEML7700_SHUTDOWN_MASK) >> VEML7700_SHUTDOWN_SHIFT;

    config->power_saving_mode = (power_saving_data & VEML7700_POWER_SAVING_MODE_MASK) >> VEML7700_POWER_SAVING_MODE_SHIFT;
    config->power_saving_enable = (power_saving_data & VEML7700_POWER_SAVING_MODE_ENABLE_MASK) >> VEML7700_POWER_SAVING_MODE_ENABLE_SHIFT;

    return ESP_OK;
}

esp_err_t veml7700_get_ambient_light(i2c_dev_t *dev, veml7700_config_t *config, uint32_t *value_lux)
{
    CHECK_ARG(dev);
    CHECK_ARG(config);
    CHECK_ARG(value_lux);

    uint16_t raw_value = 0;
    CHECK(read_port(dev, VEML7700_COMMAND_CODE_ALS, &raw_value));

    *value_lux = (raw_value * resolution(config)) / VEML7700_RESOLUTION_800MS_IT_GAIN_2_DIV;
    return ESP_OK;
}

esp_err_t veml7700_get_white_channel(i2c_dev_t *dev, veml7700_config_t *config, uint32_t *value_lux)
{
    CHECK_ARG(dev);
    CHECK_ARG(config);
    CHECK_ARG(value_lux);

    uint16_t raw_value = 0;
    CHECK(read_port(dev, VEML7700_COMMAND_CODE_WHITE, &raw_value));

    *value_lux = (raw_value * resolution(config)) / VEML7700_RESOLUTION_800MS_IT_GAIN_2_DIV;
    return ESP_OK;
}

esp_err_t veml7700_get_interrupt_status(i2c_dev_t *dev, bool *low_threshold, bool *high_threshold)
{
    CHECK_ARG(dev);
    CHECK_ARG(low_threshold);
    CHECK_ARG(high_threshold);

    uint16_t interrupt_status = 0;
    CHECK(read_port(dev, VEML7700_COMMAND_CODE_ALS_INT, &interrupt_status));

    *high_threshold = (interrupt_status & VEML7700_INTERRUPT_STATUS_HIGH_MASK) != 0;
    *low_threshold = (interrupt_status & VEML7700_INTERRUPT_STATUS_LOW_MASK) != 0;
    return ESP_OK;
}