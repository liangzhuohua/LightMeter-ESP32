#include "bsp_i2c_init.h"

static const char* TAG = "I2C_INIT";

esp_err_t i2c_init(void) {
    esp_err_t ret = ESP_OK;
    ESP_LOGI(TAG, "Initialize I2C bus");
    const i2c_config_t i2c_conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_SDA,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_SCL,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = 200 * 1000,
    };

    ret = i2c_param_config(I2C_HOST, &i2c_conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C param config FAIL: %s", esp_err_to_name(ret));
        return ret;
    }
    // 设置 I2C 超时时间
    ret = i2c_set_timeout(I2C_HOST, 0x1f); // 0x1f: 31ms
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C set timeout FAIL: %s", esp_err_to_name(ret));
        return ret;
    }
    ret = i2c_driver_install(I2C_HOST, i2c_conf.mode, 0, 0, 0);  // 修复：赋值 ret
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "I2C driver install FAIL: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "I2C init OK");
    return ret;
}