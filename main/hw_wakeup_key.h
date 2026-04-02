#ifndef HW_WAKEUP_KEY_H
#define HW_WAKEUP_KEY_H

#include "esp_err.h"
#include <stdbool.h>
#include <stdint.h>

#define WAKEUP_KEY_GPIO GPIO_NUM_9
#define WAKEUP_KEY_LONG_PRESS_MS 3000

typedef enum {
    WAKEUP_KEY_EVENT_NONE = 0,
    WAKEUP_KEY_EVENT_SHORT_PRESS,
    WAKEUP_KEY_EVENT_LONG_PRESS,
} wakeup_key_event_t;

typedef void (*wakeup_key_callback_t)(wakeup_key_event_t event);

esp_err_t hw_wakeup_key_init(void);

esp_err_t hw_wakeup_key_set_callback(wakeup_key_callback_t callback);

bool hw_wakeup_key_is_pressed(void);

bool hw_wakeup_key_check_wakeup(void);

esp_err_t hw_wakeup_key_enable_sleep_wakeup(void);

int64_t hw_wakeup_key_get_press_duration(void);

void hw_wakeup_key_reset_press(void);

#endif
