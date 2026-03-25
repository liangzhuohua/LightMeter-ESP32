#include "app_exposure_calc.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h> // for FLT_EPSILON
#include <string.h>

static const float INCIDENT_CAL = 2.5f;

/**
 * @brief 将照度(lux)转换为曝光值(EV)，使用入射光测量法
 * @param lux 环境照度值，单位：lux
 * @param iso ISO感光度值
 * @return 曝光值(EV)，如果输入无效返回-INFINITY
 * @note 计算公式：EV = log2(lux / K) + log2(iso / 100)，其中K=2.5为入射光校准常数
 */
float exposure_lux_to_ev_incident(float lux, float iso) {
    if (lux <= 0.0f || iso <= 0.0f) return -INFINITY;
    return log2f(lux / INCIDENT_CAL) + log2f(iso / 100.0f);
}

/**
 * @brief 光圈优先模式计算快门速度
 * @param lux 环境照度值，单位：lux
 * @param iso ISO感光度值
 * @param aperture 光圈值(f-number)
 * @return 快门速度(秒)，如果计算失败返回-1.0f
 * @note 计算公式：shutter = aperture² / 2^EV
 */
float exposure_aperture_priority(float lux, float iso, float aperture) {
    float ev = exposure_lux_to_ev_incident(lux, iso);
    if (ev == -INFINITY) return -1.0f;
    return powf(aperture, 2) / exp2f(ev);
}

/**
 * @brief 快门优先模式计算光圈值
 * @param lux 环境照度值，单位：lux
 * @param iso ISO感光度值
 * @param shutter 快门速度(秒)
 * @return 光圈值(f-number)，如果计算失败返回-1.0f
 * @note 计算公式：aperture = sqrt(2^EV * shutter)
 */
float exposure_shutter_priority(float lux, float iso, float shutter) {
    float ev = exposure_lux_to_ev_incident(lux, iso);
    if (ev == -INFINITY || shutter <= 0.0f) return -1.0f;
    return sqrtf(exp2f(ev) * shutter);
}

/**
 * @brief 生成指定范围内的光圈值数组
 * @param max_f 最大光圈值(光圈最小，如f/1.4)
 * @param min_f 最小光圈值(光圈最大，如f/22)
 * @param step_ev 步进值(EV)，支持0.5档或1.0档
 * @param out_count 输出参数，返回生成的光圈值数量
 * @return 光圈值数组指针，需要调用者释放内存；失败返回NULL
 * @note 生成的数组包含从max_f到min_f之间的所有光圈值
 */
float* generate_aperture(float max_f, float min_f, float step_ev, int* out_count)
{
    *out_count = 0;
    if (max_f <= 0 || min_f <= 0 || max_f > min_f || step_ev <= 0) return NULL;

    const float* src_table = NULL;
    int src_total_count = 0;
    int stride = 1;

    if (fabsf(step_ev - 0.5f) < 0.1f) {
        src_table = APERTURES_1_2;
        src_total_count = COUNT_APERTURES_1_2;
        stride = 1;
    } else if (fabsf(step_ev - 1.0f) < 0.1f) {
        src_table = APERTURES_1_3;
        src_total_count = COUNT_APERTURES_1_3;
        stride = 3; 
    } else {
        src_table = APERTURES_1_3;
        src_total_count = COUNT_APERTURES_1_3;
        stride = 1;
    }

    int start = 0;
    while (start < src_total_count && src_table[start] < max_f - 0.01f) start++;

    int end = start;
    while (end < src_total_count && src_table[end] <= min_f + 0.01f) end++;

    int capacity = (end - start) + 1;
    if (capacity <= 0) return NULL;

    float* buffer = malloc(capacity * sizeof(float));
    if (!buffer) return NULL;

    int idx = 0;
    for (int i = start; i < end; i += stride) {
        buffer[idx++] = src_table[i];
    }

    *out_count = idx;
    return buffer;
}

/**
 * @brief 生成指定范围内的快门速度数组
 * @param max_s 最快快门速度(秒)，如1/8000
 * @param min_s 最慢快门速度(秒)，如30秒
 * @param step_ev 步进值(EV)，支持0.5档或1.0档
 * @param out_count 输出参数，返回生成的快门值数量
 * @return 快门值数组指针，需要调用者释放内存；失败返回NULL
 * @note 生成的数组包含从max_s到min_s之间的所有快门值
 */
float* generate_shutter(float max_s, float min_s, float step_ev, int* out_count) {
    *out_count = 0;
    if (max_s <= 0 || min_s <= 0 || max_s < min_s || step_ev <= 0) return NULL;

    const float* src_table = NULL;
    int src_total_count = 0;
    int stride = 1;

    if (fabsf(step_ev - 0.5f) < 0.1f) {
        src_table = SHUTTERS_1_2;
        src_total_count = COUNT_SHUTTERS_1_2;
        stride = 1;
    } else if (fabsf(step_ev - 1.0f) < 0.1f) {
        src_table = SHUTTERS_1_3;
        src_total_count = COUNT_SHUTTERS_1_3;
        stride = 3; 
    } else {
        src_table = SHUTTERS_1_3;
        src_total_count = COUNT_SHUTTERS_1_3;
        stride = 1;
    }

    int start = 0;
    while (start < src_total_count && src_table[start] > max_s + 0.0001f) start++;

    int end = start;
    while (end < src_total_count && src_table[end] >= min_s - 0.0001f) end++;

    int capacity = (end - start) + 1;
    if (capacity <= 0) return NULL;

    float* buffer = malloc(capacity * sizeof(float));
    if (!buffer) return NULL;

    int idx = 0;
    for (int i = start; i < end; i += stride) {
        buffer[idx++] = src_table[i];
    }

    *out_count = idx;
    return buffer;
}

/**
 * @brief 将计算的光圈值映射到最接近的标准光圈档位
 * @param calculated_f 计算得到的光圈值
 * @param f_stop 标准光圈档位数组
 * @param f_stop_count 标准光圈档位数量
 * @return 最接近的标准光圈值，如果输入无效返回-1.0f
 * @note 使用最小距离算法找到最接近的标准档位
 */
float mapping_aperture(float calculated_f, float* f_stop, int f_stop_count) {
    if (!f_stop || f_stop_count <= 0 || calculated_f <= 0) return -1.0f;

    float min_diff = FLT_MAX;
    int best_idx = 0;

    for (int i = 0; i < f_stop_count; i++) {
        float diff = fabsf(f_stop[i] - calculated_f);
        if (diff < min_diff) {
            min_diff = diff;
            best_idx = i;
        }
    }
    return f_stop[best_idx];
}

/**
 * @brief 将计算的快门值映射到最接近的标准快门档位
 * @param calculated_s 计算得到的快门值
 * @param s_stop 标准快门档位数组
 * @param s_stop_count 标准快门档位数量
 * @return 最接近的标准快门值，如果输入无效返回-1.0f
 * @note 使用最小距离算法找到最接近的标准档位
 */
float mapping_shutter(float calculated_s, const float* s_stop, int s_stop_count) {
    if (!s_stop || s_stop_count <= 0 || calculated_s <= 0.0f) return -1.0f;

    float min_diff = FLT_MAX;
    int best_idx = 0;

    for (int i = 0; i < s_stop_count; i++) {
        float diff = fabsf(s_stop[i] - calculated_s);
        if (diff < min_diff) {
            min_diff = diff;
            best_idx = i;
        }
    }
    return s_stop[best_idx];
}

/**
 * @brief 自动曝光计算，根据不同的模式自动选择光圈和快门
 * @param lux 环境照度值，单位：lux
 * @param iso ISO感光度值
 * @param auto_mode 自动模式：EXPOSURE_AUTO(全自动)、EXPOSURE_LANDSCAPE(风光)、EXPOSURE_PORTRAIT(人像)
 * @param len 镜头参数结构体，包含光圈范围和焦距
 * @param cam 相机参数结构体，包含快门范围
 * @param aperture 输出参数，返回计算的光圈值
 * @param shutter 输出参数，返回计算的快门值
 * @param flags 输出参数，返回曝光状态标志位
 * @note 
 * - 风光模式：优先使用小光圈(f/11)以获得大景深
 * - 人像模式：优先使用大光圈以获得浅景深
 * - 全自动模式：根据环境亮度自动选择合适的光圈
 * - 会考虑镜头和相机的物理限制，并设置相应的标志位
 */
void exposure_auto(uint32_t lux, float iso, uint8_t auto_mode, LEN len, CAM cam, float* aperture, float* shutter, ExposureFlags* flags) {
    if (!aperture || !shutter || !flags || lux <= 0 || iso <= 0) {
        *aperture = -1.0f; *shutter = -1.0f; return;
    }

    memset(flags, 0, sizeof(ExposureFlags));

    float len_f_max = (len.aperture_stops && len.aperture_stop_count > 0) ? len.aperture_stops[0] : 1.4f;
    float len_f_min = (len.aperture_stops && len.aperture_stop_count > 0) ? len.aperture_stops[len.aperture_stop_count - 1] : 22.0f;
    
    float cam_s_fastest = 0.001f; 
    float cam_s_slowest = 1.0f;   
    if (cam.shutter_stops && cam.shutter_stop_count > 0) {
        float v1 = cam.shutter_stops[0];
        float v2 = cam.shutter_stops[cam.shutter_stop_count - 1];
        if (v1 < v2) { cam_s_fastest = v1; cam_s_slowest = v2; }
        else         { cam_s_fastest = v2; cam_s_slowest = v1; }
    }

    float target_f;
    switch (auto_mode) {
        case EXPOSURE_LANDSCAPE: target_f = 11.0f; break;
        case EXPOSURE_PORTRAIT:  target_f = (len_f_max < 2.0f) ? 2.0f : len_f_max; break;
        default: {
            float ev = exposure_lux_to_ev_incident(lux, iso);
            if (ev >= 15.0f) target_f = 11.0f;
            else if (ev >= 12.0f) target_f = 8.0f;
            else if (ev >= 10.0f) target_f = 5.6f;
            else if (ev >= 8.0f)  target_f = 4.0f;
            else if (ev >= 6.0f)  target_f = 2.8f;
            else target_f = len_f_max;
        } break;
    }

    if (target_f < len_f_max) target_f = len_f_max;
    if (target_f > len_f_min) target_f = len_f_min;

    float target_s = exposure_aperture_priority(lux, iso, target_f);

    if (target_s < cam_s_fastest) {
        float new_f = exposure_shutter_priority(lux, iso, cam_s_fastest);
        if (new_f > len_f_min) {
            target_f = len_f_min;
            target_s = cam_s_fastest;
            flags->overexposure = 1;
        } else {
            target_f = new_f;
            target_s = cam_s_fastest;
        }
        flags->shutter_out_of_range = 1;
    }
    else if (target_s > cam_s_slowest) {
        target_s = cam_s_slowest;
        float re_calculated_f = exposure_shutter_priority(lux, iso, target_s);

        if (re_calculated_f > len_f_min) {
            target_f = len_f_min;
            flags->overexposure = 1;
        } 
        else if (re_calculated_f < len_f_max) {
            target_f = len_f_max;
            flags->underexposure = 1;
        } 
        else {
            target_f = re_calculated_f;
        }
        flags->shutter_out_of_range = 1;
    }

    float safe_shutter = 1.0f / fmaxf(len.focal_length, 50.0f);
    if (target_s > safe_shutter) {
        flags->slow_shutter_warning = 1;
    }

    if (target_f == len_f_max || target_f == len_f_min) {
        flags->aperture_out_of_range = 1;
    }

    *aperture = (len.aperture_stops) ? 
                mapping_aperture(target_f, len.aperture_stops, len.aperture_stop_count) : target_f;
    *shutter = (cam.shutter_stops) ? 
               mapping_shutter(target_s, cam.shutter_stops, cam.shutter_stop_count) : target_s;
}


