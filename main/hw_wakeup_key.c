#include "hw_wakeup_key.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char* TAG = "hw_wakeup_key";

static wakeup_key_callback_t s_callback = NULL;
static volatile int64_t s_press_start_time = 0;
static volatile bool s_is_long_press_triggered = false;

/* GPIO中断服务：按下时记录时间，释放时判断短按/长按并调用回调 */
static void gpio_isr_handler(void* arg) {
    int level = gpio_get_level(WAKEUP_KEY_GPIO);
    int64_t now = esp_timer_get_time();

    if (level == 0) {
        s_press_start_time = now;
        s_is_long_press_triggered = false;
    } else {
        if (!s_is_long_press_triggered && s_press_start_time > 0) {
            int64_t duration = now - s_press_start_time;
            if (duration < WAKEUP_KEY_LONG_PRESS_MS * 1000) {
                if (s_callback) {
                    s_callback(WAKEUP_KEY_EVENT_SHORT_PRESS);
                }
            } else {
                if (s_callback) {
                    s_callback(WAKEUP_KEY_EVENT_LONG_PRESS);
                }
            }
        }
        s_press_start_time = 0;
    }
}

/**
 * @brief 初始化唤醒键：配置GPIO，安装ISR，检测唤醒原因
 */
esp_err_t hw_wakeup_key_init(void) {
    esp_err_t ret;

    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT1 || cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Deinitializing RTC GPIO after wakeup");
        rtc_gpio_deinit(WAKEUP_KEY_GPIO);
    }

    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << WAKEUP_KEY_GPIO),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_ANYEDGE,
    };

    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure GPIO: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Failed to install ISR service: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = gpio_isr_handler_add(WAKEUP_KEY_GPIO, gpio_isr_handler, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add ISR handler: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Wakeup key initialized on GPIO%d", WAKEUP_KEY_GPIO);
    return ESP_OK;
}

/* 设置按键事件回调函数 */
esp_err_t hw_wakeup_key_set_callback(wakeup_key_callback_t callback) {
    s_callback = callback;
    return ESP_OK;
}

/* 查询按键当前是否被按住 */
bool hw_wakeup_key_is_pressed(void) {
    return gpio_get_level(WAKEUP_KEY_GPIO) == 0;
}

/* 检查是否由唤醒键触发的深度睡眠唤醒 */
bool hw_wakeup_key_check_wakeup(void) {
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();


    ESP_LOGI(TAG, "唤醒原因: %d (EXT0=1, TIMER=4, ...)", cause);

    if (cause == ESP_SLEEP_WAKEUP_EXT0) {
        ESP_LOGI(TAG, "Woke up from deep sleep by GPIO%d (ext0)", WAKEUP_KEY_GPIO);
        return true;
    }

    if (cause == ESP_SLEEP_WAKEUP_EXT1) {
        uint64_t wakeup_pin = esp_sleep_get_ext1_wakeup_status();
        if (wakeup_pin & (1ULL << WAKEUP_KEY_GPIO)) {
            ESP_LOGI(TAG, "Woke up from deep sleep by GPIO%d (ext1)", WAKEUP_KEY_GPIO);
            return true;
        }
    }

    return false;
}

/**
 * @brief 配置唤醒键为深度睡眠唤醒源（ext0，低电平触发）
 */
esp_err_t hw_wakeup_key_enable_sleep_wakeup(void) {
    esp_err_t ret;

    ESP_LOGI(TAG, "Removing ISR handler before sleep");
    gpio_isr_handler_remove(WAKEUP_KEY_GPIO);

    // 加强配置顺序，确保稳定
    ESP_LOGI(TAG, "Configuring GPIO%d for deep sleep (active LOW)", WAKEUP_KEY_GPIO);

    rtc_gpio_init(WAKEUP_KEY_GPIO);
    rtc_gpio_set_direction(WAKEUP_KEY_GPIO, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pulldown_dis(WAKEUP_KEY_GPIO);
    rtc_gpio_pullup_en(WAKEUP_KEY_GPIO);     // 确保内部上拉

    // 重要：先让电平稳定一段时间
    vTaskDelay(pdMS_TO_TICKS(50));

    ESP_LOGI(TAG, "Enabling ext0 wakeup on GPIO%d (active LOW)", WAKEUP_KEY_GPIO);
    ret = esp_sleep_enable_ext0_wakeup(WAKEUP_KEY_GPIO, 0);  // 0 = low level 触发
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable ext0 wakeup: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Deep sleep wakeup configured successfully (button only)");
    return ESP_OK;
}

/* 获取当前按键持续按下的时间(微秒) */
int64_t hw_wakeup_key_get_press_duration(void) {
    if (s_press_start_time == 0) {
        return 0;
    }
    return esp_timer_get_time() - s_press_start_time;
}

/* 重置按键时间状态 */
void hw_wakeup_key_reset_press(void) {
    s_press_start_time = 0;
    s_is_long_press_triggered = false;
}
