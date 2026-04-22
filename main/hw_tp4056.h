#ifndef __HW_TP4056_H__
#define __HW_TP4056_H__

#include <stdint.h>

#define HW_TP4056_CHRG_GPIO         GPIO_NUM_4   // Low = charging
#define HW_TP4056_STDBY_GPIO        GPIO_NUM_5   // Low = charge complete

typedef enum {
    TP4056_STATUS_DISCHARGING = 0,  // Not charging (both pins high)
    TP4056_STATUS_CHARGING,         // Charging (CHRG low)
    TP4056_STATUS_FULL,             // Charge complete (STDBY low)
} tp4056_charge_status_t;

void hw_tp4056_init(void);
tp4056_charge_status_t hw_tp4056_get_charge_status(void);
void hw_tp4056_release_pins(void);

#endif
