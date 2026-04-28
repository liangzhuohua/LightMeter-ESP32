#ifndef __HW_VEML7700_H__
#define __HW_VEML7700_H__

#include "veml7700.h"
#include "bsp_i2c_init.h"

#define HW_VEML7700_SDA         I2C_SDA
#define HW_VEML7700_SCL         I2C_SCL
#define HW_VEML7700_I2C_NUM     I2C_HOST

/* 自适应量程级别（自上而下试探法） */
typedef enum {
    VEML7700_LEVEL_0 = 0,  /* GAIN_2,   800ms — 弱光, max ~236 lx,   分辨率 0.0036 lx/bit */
    VEML7700_LEVEL_1,      /* GAIN_1,   100ms — 常规, max ~3775 lx,  分辨率 0.0576 lx/bit */
    VEML7700_LEVEL_2,      /* GAIN_1/8, 25ms  — 强光, max ~120800 lx, 分辨率 1.8432 lx/bit */
    VEML7700_LEVEL_COUNT
} veml7700_level_t;

/* 逐级校准系数: calibrated_lux = a * raw_lux^b (power law) */
typedef struct {
    float a;  /* multiplier */
    float b;  /* exponent */
} veml7700_calib_t;

void hw_veml7700_init(uint16_t gain, uint16_t integration_time, uint16_t power_saving_mode);
void hw_veml7700_get_ambient_light(uint32_t* als);
void hw_veml7700_get_white_channel(uint32_t* white);
void hw_veml7700_shutdown(void);

/* 获取/设置当前量程级别 */
veml7700_level_t hw_veml7700_get_level(void);
uint16_t hw_veml7700_get_it_ms(void);

/* 设置某级别的校准系数（运行时标定，掉电丢失，后续可加NVS持久化） */
void hw_veml7700_set_calibration(veml7700_level_t level, float a, float b);

#endif