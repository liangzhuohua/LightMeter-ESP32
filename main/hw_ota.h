#ifndef __HW_OTA_H__
#define __HW_OTA_H__

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    HW_OTA_IDLE,
    HW_OTA_AP_STARTING,
    HW_OTA_AP_READY,
    HW_OTA_UPLOADING,
    HW_OTA_VERIFYING,
    HW_OTA_SUCCESS,
    HW_OTA_FAIL,
} hw_ota_state_t;

typedef void (*hw_ota_progress_cb_t)(hw_ota_state_t state, int progress);

void hw_ota_register_progress_cb(hw_ota_progress_cb_t cb);
esp_err_t hw_ota_start(void);
void hw_ota_stop(void);
hw_ota_state_t hw_ota_get_state(void);

#endif
