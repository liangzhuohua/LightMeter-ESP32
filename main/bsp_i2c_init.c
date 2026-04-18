#include "bsp_i2c_init.h"
#include "i2cdev.h"
#include "driver/rtc_io.h"

static const char* TAG = "I2C_INIT";

static i2c_master_bus_handle_t s_bus_handle = NULL;
static i2c_dev_t s_temp_dev = {0};

esp_err_t i2c_init(void) {
    ESP_LOGI(TAG, "Initialize I2C bus");

    esp_err_t ret = i2cdev_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2cdev_init FAIL: %s", esp_err_to_name(ret));
        return ret;
    }

    s_temp_dev.port = I2C_HOST;
    s_temp_dev.addr = 0x00;
    s_temp_dev.cfg.sda_io_num = I2C_SDA;
    s_temp_dev.cfg.scl_io_num = I2C_SCL;
    s_temp_dev.cfg.sda_pullup_en = 1;
    s_temp_dev.cfg.scl_pullup_en = 1;

    ret = i2c_dev_create_mutex(&s_temp_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i2c_dev_create_mutex FAIL: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = i2c_dev_check_present(&s_temp_dev);
    if (ret != ESP_OK && ret != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "I2C bus probe returned: %s", esp_err_to_name(ret));
    }

    s_bus_handle = i2cdev_get_bus_handle(I2C_HOST);
    ESP_LOGI(TAG, "I2C init OK, bus handle: %p", s_bus_handle);

// 在 i2c_init() 之后添加
    for (uint8_t addr = 1; addr < 127; addr++) {
        s_temp_dev.addr = addr << 1;
        if (i2c_dev_check_present(&s_temp_dev) == ESP_OK) {
            ESP_LOGI(TAG, "Found I2C device at address: 0x%02X", addr);
        }
    }

    return ESP_OK;
}

i2c_master_bus_handle_t i2c_get_bus_handle(void) {
    if (s_bus_handle == NULL) {
        s_bus_handle = i2cdev_get_bus_handle(I2C_HOST);
    }
    return s_bus_handle;
}

void i2c_release_pins(void) {
    ESP_LOGI(TAG, "Releasing I2C pins for deep sleep");
    gpio_num_t i2c_pins[] = { I2C_SDA, I2C_SCL };
    for (int i = 0; i < sizeof(i2c_pins) / sizeof(i2c_pins[0]); i++) {
        rtc_gpio_init(i2c_pins[i]);
        rtc_gpio_set_direction(i2c_pins[i], RTC_GPIO_MODE_INPUT_ONLY);
        rtc_gpio_pulldown_dis(i2c_pins[i]);
        rtc_gpio_pullup_dis(i2c_pins[i]);
    }
}
