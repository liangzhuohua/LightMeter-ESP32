#include "hw_veml7700.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "hw_veml7700";

i2c_dev_t veml7700_device;
veml7700_config_t veml7700_configuration;

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

    /* 设置配置参数
     * 选择增益 1/8 以获得最高的 resolution，但传感器很可能不会过饱和
     */
    veml7700_configuration.gain = gain;

    /* 设置积分时间来积分光值。时间越长，分辨率越精细，但更容易过饱和
     */

    veml7700_configuration.integration_time = integration_time;

    // 中断未使用
    veml7700_configuration.persistence_protect = VEML7700_PERSISTENCE_PROTECTION_4;
    veml7700_configuration.interrupt_enable = 1;
    veml7700_configuration.shutdown = 0;

    /* 设置省电模式。这会减少重复测量。当禁用时，传感器会连续执行一个测量周期（这里：100ms 积分时间），设置省电模式会在测量期间添加 1000ms 睡眠。
     */
    veml7700_configuration.power_saving_mode = power_saving_mode;
    veml7700_configuration.power_saving_enable = 1;

    // 将配置写入设备
    ESP_ERROR_CHECK(veml7700_set_config(&veml7700_device, &veml7700_configuration));

    /* 读取回配置用于测试目的。此步骤在正常应用中不需要
     */
    veml7700_config_t veml7700_configuration_readback;
    ESP_ERROR_CHECK(veml7700_get_config(&veml7700_device, &veml7700_configuration_readback));

    if (compare_configuration(&veml7700_configuration_readback,  // 修复：第二个参数
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
    veml7700_get_ambient_light(&veml7700_device, &veml7700_configuration, als);
}

void hw_veml7700_get_white_channel(uint32_t* white) {
    veml7700_get_white_channel(&veml7700_device, &veml7700_configuration, white);
}

void hw_veml7700_shutdown(void) {
    ESP_LOGI(TAG, "Shutting down VEML7700");
    veml7700_configuration.shutdown = 1;
    veml7700_set_config(&veml7700_device, &veml7700_configuration);
}
