#ifndef __BSP_I2C_INIT_H__
#define __BSP_I2C_INIT_H__

#include "esp_err.h"
#include "driver/i2c.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define I2C_HOST    0
#define I2C_SCL     GPIO_NUM_2
#define I2C_SDA     GPIO_NUM_1

esp_err_t i2c_init(void);


#endif // !__I2C_INIT_H__