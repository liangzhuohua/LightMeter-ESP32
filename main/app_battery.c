#include "app_battery.h"
#include "hw_max17055.h"
#include "hw_tp4056.h"
#include "esp_log.h"

static const char *TAG = "app_battery";

void app_battery_init(void)
{
    hw_max17055_init();
    hw_tp4056_init();
    ESP_LOGI(TAG, "Battery management initialized");
}

void app_battery_get_info(float *soc_pct, float *voltage_mv, battery_status_t *status)
{
    if (soc_pct) {
        hw_max17055_get_soc(soc_pct);
    }
    if (voltage_mv) {
        hw_max17055_get_vcell(voltage_mv);
    }
    if (status) {
        tp4056_charge_status_t tp4056_status = hw_tp4056_get_charge_status();
        switch (tp4056_status) {
            case TP4056_STATUS_CHARGING:
                *status = BATTERY_STATUS_CHARGING;
                break;
            case TP4056_STATUS_FULL:
                *status = BATTERY_STATUS_FULL;
                break;
            default:
                *status = BATTERY_STATUS_DISCHARGING;
                break;
        }
    }
}

void app_battery_sleep(void)
{
    hw_max17055_sleep();
    hw_max17055_release_pins();
    hw_tp4056_release_pins();
}
