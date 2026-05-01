#include "hw_tp4056.h"
#include <driver/gpio.h>
#include <driver/rtc_io.h>
#include "esp_log.h"

static const char *TAG = "hw_tp4056";

/**
 * @brief 初始化TP4056充电检测芯片（配置CHRG和STDBY引脚为输入）
 */
void hw_tp4056_init(void)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << HW_TP4056_CHRG_GPIO) | (1ULL << HW_TP4056_STDBY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    ESP_LOGI(TAG, "TP4056 charging detector initialized (CHRG=GPIO%d, STDBY=GPIO%d)",
             HW_TP4056_CHRG_GPIO, HW_TP4056_STDBY_GPIO);
}

/**
 * @brief 读取TP4056充电状态（CHRG低=充电中，STDBY低=充满，都高=未充电）
 */
tp4056_charge_status_t hw_tp4056_get_charge_status(void)
{
    int chrg_level = gpio_get_level(HW_TP4056_CHRG_GPIO);
    int stdby_level = gpio_get_level(HW_TP4056_STDBY_GPIO);

    if (chrg_level == 0) {
        return TP4056_STATUS_CHARGING;
    } else if (stdby_level == 0) {
        return TP4056_STATUS_FULL;
    } else {
        return TP4056_STATUS_DISCHARGING;
    }
}

/* 释放CHRG/STDBY引脚（为深度睡眠做准备，防止漏电） */
void hw_tp4056_release_pins(void)
{
    ESP_LOGI(TAG, "Releasing CHRG/STDBY pins for deep sleep");

    rtc_gpio_init(HW_TP4056_CHRG_GPIO);
    rtc_gpio_set_direction(HW_TP4056_CHRG_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(HW_TP4056_CHRG_GPIO);
    rtc_gpio_pullup_dis(HW_TP4056_CHRG_GPIO);

    rtc_gpio_init(HW_TP4056_STDBY_GPIO);
    rtc_gpio_set_direction(HW_TP4056_STDBY_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(HW_TP4056_STDBY_GPIO);
    rtc_gpio_pullup_dis(HW_TP4056_STDBY_GPIO);
}
