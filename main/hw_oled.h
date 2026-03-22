#ifndef __HW_OLED_H__
#define __HW_OLED_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "driver/spi_master.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_lcd_touch_cst820.h"
#include "lvgl.h"
#include "lv_demos.h"
#include "esp_lcd_qspi_amoled.h"

#include "bsp_i2c_init.h"

#define LCD_HOST    SPI2_HOST
#define TOUCH_HOST  I2C_HOST

#if CONFIG_LV_COLOR_DEPTH == 32
#define LCD_BIT_PER_PIXEL       (24)
#elif CONFIG_LV_COLOR_DEPTH == 16
#define LCD_BIT_PER_PIXEL       (16)
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////// Please update the following configuration according to your LCD spec //////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// 鱼鹰光电屏幕验证底板
#define EXAMPLE_PIN_NUM_LCD_CS            (GPIO_NUM_40)
#define EXAMPLE_PIN_NUM_LCD_PCLK          (GPIO_NUM_21) 
#define EXAMPLE_PIN_NUM_LCD_DATA0         (GPIO_NUM_47)
#define EXAMPLE_PIN_NUM_LCD_DATA1         (GPIO_NUM_45)
#define EXAMPLE_PIN_NUM_LCD_DATA2         (GPIO_NUM_38)
#define EXAMPLE_PIN_NUM_LCD_DATA3         (GPIO_NUM_39)
#define EXAMPLE_PIN_NUM_LCD_RST           (GPIO_NUM_41)


// ESP32S3_AMOLED_触摸
#define EXAMPLE_PIN_NUM_TOUCH_SCL         (I2C_SCL )
#define EXAMPLE_PIN_NUM_TOUCH_SDA         (I2C_SDA )
#define EXAMPLE_PIN_NUM_TOUCH_RST         (GPIO_NUM_13)
#define EXAMPLE_PIN_NUM_TOUCH_INT         (GPIO_NUM_14)



#define EXAMPLE_LVGL_BUF_HEIGHT        (EXAMPLE_LCD_V_RES / 10)
#define EXAMPLE_LVGL_TICK_PERIOD_MS    2
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1
#define EXAMPLE_LVGL_TASK_STACK_SIZE   (4 * 1024)
#define EXAMPLE_LVGL_TASK_PRIORITY     2


#define EXAMPLE_LCD_H_RES              460
#define EXAMPLE_LCD_V_RES              460
#define EXAMPLE_LCD_X_GAP              10
#define EXAMPLE_LCD_Y_GAP              0
#define AMOLED_QSPI_MAX_PCLK           40 * 1000 * 1000

static const qspi_amoled_lcd_init_cmd_t lcd_init_cmds[] = {
  {0xFE, (uint8_t []){0x00}, 1, 0},
  {0x11, (uint8_t []){0x00}, 0, 120},// 退出睡眠模式
  {0x35, (uint8_t []){0x00}, 1, 0},// 开启撕裂效果
  {0xFE, (uint8_t []){0x00}, 1, 0},
  {0xC4, (uint8_t []){0x80}, 1, 0},// SPI 模式控制
  // {0x36, (uint8_t []){0x00}, 1, 0},// 设置内存数据访问控制
  {0x3A, (uint8_t []){0x55}, 1, 0},//// 设置像素格式 16位
  {0x53, (uint8_t []){0x20}, 1, 0},// 设置 CTRL 显示1
  {0x63, (uint8_t []){0xFF}, 1, 0},// 设置 HBM 模式下的亮度值
  {0x2A, (uint8_t []){0x00, 0x00, 0x01, 0xBF}, 4, 0},
  {0x2B, (uint8_t []){0x00, 0x00, 0x01, 0x6F}, 4, 0},
  {0x29, (uint8_t []){0x00}, 0, 60},// 打开显示器
  {0x51, (uint8_t []){0xFF}, 1, 0},// 设置正常模式下的亮度值
  {0x58, (uint8_t []){0x07}, 1, 10},// 设置正常模式下的亮度值
};


#define TOUCH_IO_I2C_CONFIG    ESP_LCD_TOUCH_IO_I2C_CST820_CONFIG  
#define esp_lcd_touch_new_i2c  esp_lcd_touch_new_i2c_cst820

void oled_lvgl_init(void);
bool example_lvgl_lock(int timeout_ms);
void example_lvgl_unlock(void);
void oled_set_brightness(uint8_t brightness);

#endif // !__BSP_OLED_H__

