#include "app_battery.h"
#include "hw_max17055.h"
#include "hw_tp4056.h"
#include "esp_log.h"

static const char *TAG = "app_battery";

// 跟踪上一次的充电状态，用于检测状态变化
static battery_status_t last_status = BATTERY_STATUS_DISCHARGING;

/**
 * @brief 初始化电池管理系统（MAX17055电量计 + TP4056充电检测）
 */
void app_battery_init(void)
{
    hw_max17055_init();
    hw_tp4056_init();
    ESP_LOGI(TAG, "Battery management initialized");
}

/**
 * @brief 获取电池综合信息（SOC、电压、充电状态）
 * @param soc_pct 输出：电池电量百分比
 * @param voltage_mv 输出：电池电压(mV)
 * @param status 输出：充电状态（放电/充电/充满）
 */
void app_battery_get_info(float *soc_pct, float *voltage_mv, battery_status_t *status)
{
    tp4056_charge_status_t tp4056_status = hw_tp4056_get_charge_status();
    battery_status_t current_status;

    switch (tp4056_status) {
        case TP4056_STATUS_CHARGING:
            current_status = BATTERY_STATUS_CHARGING;
            break;
        case TP4056_STATUS_FULL:
            current_status = BATTERY_STATUS_FULL;
            break;
        default:
            current_status = BATTERY_STATUS_DISCHARGING;
            break;
    }

    // 检测从非充满状态变为充满状态
    if (current_status == BATTERY_STATUS_FULL && last_status != BATTERY_STATUS_FULL) {
        ESP_LOGI(TAG, "Battery just became full, calibrating MAX17055");
        hw_max17055_force_full(0);  // 使用当前学习的满充容量
    }
    last_status = current_status;

    if (soc_pct) {
        hw_max17055_get_soc(soc_pct);
    }
    if (voltage_mv) {
        hw_max17055_get_vcell(voltage_mv);
    }
    if (status) {
        *status = current_status;
    }
}

/**
 * @brief 通知电池已充满，触发MAX17055满充校准
 */
void app_battery_notify_full(void)
{
    ESP_LOGI(TAG, "External full charge notification received");
    hw_max17055_force_full(0);
}

/**
 * @brief 电池模块进入休眠（释放GPIO引脚，准备深度睡眠）
 */
void app_battery_sleep(void)
{
    hw_max17055_sleep();
    hw_max17055_release_pins();
    hw_tp4056_release_pins();
}
