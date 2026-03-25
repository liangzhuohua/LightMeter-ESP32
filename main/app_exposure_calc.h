#ifndef __APP_EXPOSURE_CALC_H__
#define __APP_EXPOSURE_CALC_H__

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    float* aperture_stops;          // 镜头光圈表
    int aperture_stop_count;        // 光圈表数
    float focal_length;             // 焦距
} LEN;

typedef struct {
    float* shutter_stops;           // 相机快门表
    int shutter_stop_count;         // 快门表数
    float flash_sync_shutter;       // 闪光同步
} CAM;

// ==================== 1/3 档标准光圈表 (用于 0.3EV 和 1.0EV) ====================
static const float APERTURES_1_3[] = {
    0.50f, 0.56f, 0.63f, 
    0.70f, 0.80f, 0.90f,
    1.0f,  1.1f,  1.2f, 
    1.4f,  1.6f,  1.8f,
    2.0f,  2.2f,  2.5f, 
    2.8f,  3.2f,  3.5f,
    4.0f,  4.5f,  5.0f, 
    5.6f,  6.3f,  7.1f,
    8.0f,  9.0f,  10.0f, 
    11.0f, 13.0f, 14.0f,
    16.0f, 18.0f, 20.0f, 
    22.0f, 25.0f, 29.0f,
    32.0f, 36.0f, 40.0f, 
    45.0f, 51.0f, 57.0f,
    64.0f, 72.0f, 80.0f, 
    91.0f, 102.0f, 114.0f,
    128.0f
};
static const int COUNT_APERTURES_1_3 = sizeof(APERTURES_1_3) / sizeof(float);

// ==================== 1/2 档标准光圈表 (用于 0.5EV) ====================
// 注意：f/1.7, f/2.4, f/3.3, f/4.8, f/6.7 等数值只存在于半档表中
static const float APERTURES_1_2[] = {
    0.70f, 0.85f, 
    1.0f,  1.2f,  1.4f,  1.7f, 
    2.0f,  2.4f,  2.8f,  3.4f, // 有时标记为 3.3 或 3.5，这里取 3.4
    4.0f,  4.8f,  5.6f,  6.7f, 
    8.0f,  9.5f, 11.0f, 13.5f, 
    16.0f, 19.0f, 22.0f, 27.0f, 
    32.0f, 38.0f, 45.0f, 54.0f, 
    64.0f, 76.0f, 91.0f
};
static const int COUNT_APERTURES_1_2 = sizeof(APERTURES_1_2) / sizeof(float);


// ==================== 1/3 档标准快门表 ====================
static const float SHUTTERS_1_3[] = {
    30.0f, 25.0f, 20.0f, 15.0f, 13.0f, 10.0f, 8.0f, 6.0f, 5.0f, 4.0f, 3.2f, 2.5f,
    2.0f, 1.6f, 1.3f, 1.0f, 0.8f, 0.6f, 0.5f, 0.4f, 0.3f,
    1.0f/4, 1.0f/5, 1.0f/6, 1.0f/8, 1.0f/10, 1.0f/13, 1.0f/15, 1.0f/20, 1.0f/25,
    1.0f/30, 1.0f/40, 1.0f/50, 1.0f/60, 1.0f/80, 1.0f/100, 1.0f/125, 1.0f/160, 1.0f/200,
    1.0f/250, 1.0f/320, 1.0f/400, 1.0f/500, 1.0f/640, 1.0f/800, 1.0f/1000, 1.0f/1250, 1.0f/1600,
    1.0f/2000, 1.0f/2500, 1.0f/3200, 1.0f/4000, 1.0f/5000, 1.0f/6400, 1.0f/8000
};
static const int COUNT_SHUTTERS_1_3 = sizeof(SHUTTERS_1_3) / sizeof(float);

// ==================== 1/2 档标准快门表 ====================
static const float SHUTTERS_1_2[] = {
    30.0f, 20.0f, 15.0f, 10.0f, 8.0f, 6.0f, 4.0f, 3.0f, 2.0f, 1.5f, 1.0f, 0.7f, 0.5f, 0.3f,
    1.0f/4, 1.0f/6, 1.0f/8, 1.0f/10, 1.0f/15, 1.0f/20, 1.0f/30, 1.0f/45, 1.0f/60, 1.0f/90,
    1.0f/125, 1.0f/180, 1.0f/250, 1.0f/350, 1.0f/500, 1.0f/750, 1.0f/1000, 1.0f/1500,
    1.0f/2000, 1.0f/3000, 1.0f/4000, 1.0f/6000, 1.0f/8000
};
static const int COUNT_SHUTTERS_1_2 = sizeof(SHUTTERS_1_2) / sizeof(float);


enum {
    EXPOSURE_MANUAL = 0,       // 手动模式
    EXPOSURE_AUTO = 1,          // 全自动模式
    EXPOSURE_LANDSCAPE = 2,    // 风光模式
    EXPOSURE_PORTRAIT = 3      // 人像模式
};

typedef struct {
    uint8_t aperture_out_of_range : 1;    // 光圈超出镜头范围
    uint8_t shutter_out_of_range : 1;    // 快门超出相机范围
    uint8_t overexposure : 1;             // 过曝（即使使用最小光圈和最快快门）
    uint8_t underexposure : 1;            // 欠曝（即使使用最大光圈和最慢快门）
    uint8_t slow_shutter_warning : 1;     // 快门过慢警告（建议使用三脚架）
} ExposureFlags;




// 函数声明
float exposure_lux_to_ev_incident(float lux, float iso);
float exposure_aperture_priority(float lux, float iso, float ev_compensation, float aperture);
float exposure_shutter_priority(float lux, float iso, float ev_compensation, float shutter);
float* generate_aperture(float max_f, float min_f, float step_ev, int* out_count);
float* generate_shutter(float max_s, float min_s, float step_ev, int* out_count);
float mapping_aperture(float calculated_f, float* f_stop, int f_stop_count);
float mapping_shutter(float calculated_s, const float* s_stop, int s_stop_count);
void exposure_auto(uint32_t lux, float iso, uint8_t auto_mode, LEN len, CAM cam, float ev_compensation, float* aperture, float* shutter, ExposureFlags* flags);



#endif