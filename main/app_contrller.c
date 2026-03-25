#include "app_contrller.h"
#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "app_exposure_calc.h"
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static const char* TAG = "app_contrller";

static void task_get_lux_value(void* pvParameters)
{
    while (1)
    {
        uint32_t als = 0;
        uint32_t white = 0;
        hw_veml7700_get_ambient_light(&als);
        hw_veml7700_get_white_channel(&white);
        ESP_LOGI(TAG, "ALS: %u lx", als);
        ESP_LOGI(TAG, "WHITE: %u lx", white);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}



void app_contrller_init(void)
{
    xTaskCreate(task_get_lux_value, "task_get_lux_value", 2048, NULL, 5, NULL);
    // xTaskCreate();

}