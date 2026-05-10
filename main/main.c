#include <stdio.h>
#include "hw_oled.h"
#include "bsp_i2c_init.h"
#include "hw_veml7700.h"
#include "app_ui.h"
#include "app_controller.h"
#include "nvs_flash.h"
#include "hw_wifi.h"
#include "esp_log.h"


static const char* TAG = "main";

/**
 * @brief 应用主入口：初始化I2C、VEML7700光传感器、NVS、OLED/LVGL显示，然后启动控制器
 */
void app_main(void)
{
    i2c_init();

    hw_veml7700_init(VEML7700_POWER_SAVING_MODE_500MS);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    oled_lvgl_init();
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


    app_controller_init();
}
