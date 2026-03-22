#include <stdio.h>
#include "hw_oled.h"
#include "hw_sdcard.h"
#include "bsp_i2c_init.h"
#include "hw_veml7700.h"
#include "app_ui.h"

static const char* TAG = "main";

void app_main(void)
{
    i2c_init();

    hw_veml7700_init(VEML7700_GAIN_1, VEML7700_INTEGRATION_TIME_100MS, VEML7700_POWER_SAVING_MODE_500MS);
    oled_lvgl_init();
    // bsp_sd_init();
    // Lock the mutex due to the LVGL APIs are not thread-safe
    if (example_lvgl_lock(-1)) {

        // lv_obj_t *label = lv_label_create(lv_scr_act());
        // lv_label_set_text(label, "Hello AMOLED");
        // lv_obj_center(label);
        ui_exposure_init();
        // lv_demo_widgets();      /* A widgets example */
        // lv_demo_music();        /* A modern, smartphone-like music player demo. */
        // lv_demo_stress();       /* A stress test for LVGL. */
        // lv_demo_benchmark();    /* A demo to measure the performance of LVGL or to compare different settings. */
        // Release the mutex
        example_lvgl_unlock();
    }
 
    uint32_t als = 0;
    uint32_t white = 0;
    bool low_threshold = false;
    bool high_threshold = false;
    uint8_t brightness = 0;

    while (1)
    {
        hw_veml7700_get_ambient_light(&als);
        hw_veml7700_get_white_channel(&white);
        ESP_LOGI(TAG, "ALS: %u lx", als);
        ESP_LOGI(TAG, "WHITE: %u lx", white);

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

