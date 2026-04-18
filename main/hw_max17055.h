#ifndef __HW_MAX17055_H__
#define __HW_MAX17055_H__

#include "max17055.h"
#include "bsp_i2c_init.h"

#define HW_MAX17055_SDA             I2C_SDA
#define HW_MAX17055_SCL             I2C_SCL
#define HW_MAX17055_I2C_NUM         I2C_HOST
#define HW_MAX17055_ALRT_GPIO       GPIO_NUM_6

#define HW_MAX17055_BATT_CAP_MAH    300
#define HW_MAX17055_RSENSE_MOHM     20.0f
#define HW_MAX17055_VCHARGE         4.2f
#define HW_MAX17055_VEMPTY          3.3f
#define HW_MAX17055_VRECOVERY       3.4f

void hw_max17055_init(void);
void hw_max17055_get_vcell(float *voltage_mv);
void hw_max17055_get_avg_vcell(float *voltage_mv);
void hw_max17055_get_current(float *current_ma);
void hw_max17055_get_avg_current(float *current_ma);
void hw_max17055_get_soc(float *soc_pct);
void hw_max17055_get_rep_cap(float *cap_mah);
void hw_max17055_get_temperature(float *temp_c);
void hw_max17055_get_tte(float *tte_s);
void hw_max17055_get_ttf(float *ttf_s);
void hw_max17055_get_full_cap(float *cap_mah);
void hw_max17055_get_cycles(uint16_t *cycles);
void hw_max17055_get_age(uint8_t *age_pct);
bool hw_max17055_is_charging(void);
void hw_max17055_get_status(uint16_t *status);
void hw_max17055_sleep(void);

#endif
