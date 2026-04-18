#ifndef __BSP_I2C_INIT_H__
#define __BSP_I2C_INIT_H__

#include "esp_err.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"

#define I2C_HOST        0
#define I2C_SCL         GPIO_NUM_2
#define I2C_SDA         GPIO_NUM_1
#define I2C_CLK_SPEED   400000

esp_err_t i2c_init(void);
i2c_master_bus_handle_t i2c_get_bus_handle(void);
void i2c_release_pins(void);

#endif
