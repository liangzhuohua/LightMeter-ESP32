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
extern lv_obj_t* main_obj_mode_select;         // 模式选择容器

extern lv_obj_t* weather_icon;                 // 天气图标

// 获取当前选中的相机和镜头卡片
lv_obj_t* app_ui_get_cam_selected_card(void);
lv_obj_t* app_ui_get_len_selected_card(void);

// 获取当前选择的拍摄模式
uint8_t app_ui_get_selected_mode(void);

// 设置当前选择的拍摄模式
void app_ui_set_selected_mode(uint8_t mode_idx);

// 获取相机/镜头卡片容器
lv_obj_t* app_ui_get_cam_container(void);
lv_obj_t* app_ui_get_len_container(void);

// 获取相机/镜头卡片数量
uint16_t app_ui_get_cam_count(void);
uint16_t app_ui_get_len_count(void);

// 获取选中的相机/镜头索引
int app_ui_get_cam_selected_index(void);
int app_ui_get_len_selected_index(void);

// 设置选中的相机/镜头索引
void app_ui_set_cam_selected_index(int idx);
void app_ui_set_len_selected_index(int idx);

// 获取滚轮选中索引
uint16_t app_ui_get_roller_selected(lv_obj_t* roller);

// 辅助函数：生成快门选项字符串（供Port层使用）
char* app_ui_generate_shutter_options(const float* shutter_array, int count, int stride);

// 辅助函数：生成光圈选项字符串（供Port层使用）
char* app_ui_generate_aperture_options(const float* aperture_array, int count, int stride);

// ──────────────────────────────────────────────
// 相机/镜头卡片创建函数（供 NVS 恢复使用）
// ──────────────────────────────────────────────
void app_ui_create_cam_card(const char* name, int step_type, int min_idx, int max_idx, const char* flash_sync);
void app_ui_create_len_card(const char* name, int step_type, int min_idx, int max_idx, const char* focal_length);

// ──────────────────────────────────────────────
// WiFi UI 函数（内部使用，请通过 app_ui_wifi_port 访问）
// ──────────────────────────────────────────────
void add_wifi_card(const char *wifi_name, int signal_strength);
void app_ui_wifi_on_connecting(const char *ssid);
void app_ui_wifi_on_connected(const char *ssid);
void app_ui_wifi_on_disconnected(const char *ssid);
void app_ui_wifi_on_connect_failed(const char *ssid, int reason);
void app_ui_wifi_set_enabled(bool enabled);

#endif // __APP_UI_H__
