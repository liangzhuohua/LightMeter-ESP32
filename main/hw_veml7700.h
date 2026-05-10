#ifndef __HW_VEML7700_H__
#define __HW_VEML7700_H__

#include "veml7700.h"
#include "bsp_i2c_init.h"

#define HW_VEML7700_SDA         I2C_SDA
#define HW_VEML7700_SCL         I2C_SCL
#define HW_VEML7700_I2C_NUM     I2C_HOST

/* 自适应量程级别（遵循 Vishay AN 84323 推荐流程）
 *
 * 默认从 L2 (GAIN_1/8, IT=100ms) 起步，避免饱和。
 * 切换规则基于 raw counts（非 lux），与官方流程图一致：
 *   - ≤100 counts → 提高灵敏度（升增益/加长积分时间）
 *   - >10000 counts（在 GAIN_1/8 时）→ 缩短积分时间
 */
typedef enum {
    VEML7700_LEVEL_0 = 0,  /* GAIN_2,    800ms — 弱光 ~0.008–84 lx,   分辨率 0.0042 lx/cnt */
    VEML7700_LEVEL_1,      /* GAIN_1/4,  100ms — 室内 ~27–17k lx,     分辨率 0.2688 lx/cnt */
    VEML7700_LEVEL_2,      /* GAIN_1/8,  100ms — 户外 ~54–35k lx,     分辨率 0.5376 lx/cnt (默认起步) */
    VEML7700_LEVEL_3,      /* GAIN_1/8,   50ms — 强光 ~108–70k lx,    分辨率 1.0752 lx/cnt */
    VEML7700_LEVEL_4,      /* GAIN_1/8,   25ms — 极强光 ~215–141k lx, 分辨率 2.1504 lx/cnt */
    VEML7700_LEVEL_COUNT
} veml7700_level_t;

void hw_veml7700_init(uint16_t power_saving_mode);
void hw_veml7700_get_ambient_light(uint32_t *als);
void hw_veml7700_get_white_channel(uint32_t *white);
void hw_veml7700_shutdown(void);

/* 获取当前量程级别和积分时间 */
veml7700_level_t hw_veml7700_get_level(void);
uint16_t hw_veml7700_get_it_ms(void);

/* 设置盖板玻璃透光率补偿系数（默认 1.0）
 * 例如暗色盖板透光 10%，factor = 10.0 (= 1/0.1) */
void hw_veml7700_set_transmission(float factor);

#endif
