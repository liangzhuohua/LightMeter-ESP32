#ifndef __HW_VEML7700_H__
#define __HW_VEML7700_H__

#include "veml7700.h"
#include "bsp_i2c_init.h"

#define HW_VEML7700_SDA         I2C_SDA
#define HW_VEML7700_SCL         I2C_SCL
#define HW_VEML7700_I2C_NUM     I2C_HOST

void hw_veml7700_init(uint16_t gain, uint16_t integration_time, uint16_t power_saving_mode);
void hw_veml7700_get_ambient_light(uint32_t* als);
void hw_veml7700_get_white_channel(uint32_t* white);
void hw_veml7700_shutdown(void);

#endif