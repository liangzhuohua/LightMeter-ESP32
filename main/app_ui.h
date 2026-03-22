#ifndef __APP_UI_H__
#define __APP_UI_H__

#include "lvgl.h"

// UI 初始化函数
void ui_exposure_init(void);

// 全局 UI 对象指针（供外部更新使用）
extern lv_obj_t* main_table_time;              // 时间
extern lv_obj_t* main_table_status;            // 信号、电量
extern lv_obj_t* main_label_cam;               // 相机名字
extern lv_obj_t* main_label_len;               // 镜头名字
extern lv_obj_t* main_roller_shutter;          // 快门滚轮
extern lv_obj_t* main_roller_aperture;         // 光圈滚轮
extern lv_obj_t* main_roller_iso;              // ISO 值
extern lv_obj_t* main_roller_ev;               // EV 值
extern lv_obj_t* main_label_lux_value;         // 光照强度数值显示

// 获取当前选中的相机和镜头卡片
lv_obj_t* app_ui_get_cam_selected_card(void);
lv_obj_t* app_ui_get_len_selected_card(void);

// 获取当前选择的拍摄模式
uint8_t app_ui_get_selected_mode(void);

// 辅助函数：生成快门选项字符串（供Port层使用）
char* app_ui_generate_shutter_options(const float* shutter_array, int count, int stride);

// 辅助函数：生成光圈选项字符串（供Port层使用）
char* app_ui_generate_aperture_options(const float* aperture_array, int count, int stride);

#endif // __APP_UI_H__
