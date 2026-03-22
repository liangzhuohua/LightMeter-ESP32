#include "app_exposure_calc.h"
#include <stdio.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <float.h> // for FLT_EPSILON

float exposure_lux_to_ev_incident(float lux, float iso) {
    if (lux <= 0.0f || iso <= 0.0f) return -INFINITY;
    return log2f(lux / INCIDENT_CAL) + log2f(iso / 100.0f);
}

float exposure_aperture_priority(float lux, float iso, float aperture) {
    float ev = exposure_lux_to_ev_incident(lux, iso);
    if (ev == -INFINITY) return -1.0f;
    return powf(aperture, 2) / exp2f(ev);
}

// 修复后的快门优先计算
float exposure_shutter_priority(float lux, float iso, float shutter) {
    float ev = exposure_lux_to_ev_incident(lux, iso);
    if (ev == -INFINITY || shutter <= 0.0f) return -1.0f;
    // 公式: N = sqrt( t * 2^EV )
    return sqrtf(exp2f(ev) * shutter);
}

/**
 * @name             generate_aperture
 * @details          支持 1/3, 1/2, 1.0 档步进
 */
float* generate_aperture(float max_f, float min_f, float step_ev, int* out_count)
{
    *out_count = 0;
    if (max_f <= 0 || min_f <= 0 || max_f > min_f || step_ev <= 0) return NULL;

    const float* src_table = NULL;
    int src_total_count = 0;
    int stride = 1;

    // 智能选择母表和步长
    if (fabsf(step_ev - 0.5f) < 0.1f) {
        // 半档 -> 使用专用半档表
        src_table = APERTURES_1_2;
        src_total_count = COUNT_APERTURES_1_2;
        stride = 1;
    } else if (fabsf(step_ev - 1.0f) < 0.1f) {
        // 整档 -> 使用 1/3 表，每 3 个跳一次 (1.0 -> 1.4 -> 2.0)
        src_table = APERTURES_1_3;
        src_total_count = COUNT_APERTURES_1_3;
        stride = 3; 
    } else {
        // 默认/0.33 -> 使用 1/3 表，步长 1
        src_table = APERTURES_1_3;
        src_total_count = COUNT_APERTURES_1_3;
        stride = 1;
    }

    // 1. 找起点 (第一个 >= max_f)
    int start = 0;
    // 加上一个小 epsilon 防止浮点数相等判断失败
    while (start < src_total_count && src_table[start] < max_f - 0.01f) start++;

    // 2. 找终点 (第一个 <= min_f)
    int end = start;
    while (end < src_total_count && src_table[end] <= min_f + 0.01f) end++;

    // 3. 按照步长收集
    int capacity = (end - start) + 1; // 预估最大值
    if (capacity <= 0) return NULL;

    float* buffer = malloc(capacity * sizeof(float));
    if (!buffer) return NULL;

    int idx = 0;
    // 注意：这里需要对齐整档。如果用户选整档，但 start 指向了 f/1.1，应该往后找最近的整档吗？
    // 为了简单且符合物理镜头，我们从 start 开始按步长切分。
    // 如果镜头最大光圈就是 f/1.8 (非整档)，那它下一档确实应该是 f/1.8 + 1EV = f/3.5。
    // 所以直接 +stride 是正确的逻辑。
    for (int i = start; i < end; i += stride) {
        buffer[idx++] = src_table[i];
    }

    *out_count = idx;
    return buffer;
}

/**
 * @name             generate_shutter
 * @details          支持 1/3, 1/2, 1.0 档步进
 */
float* generate_shutter(float max_s, float min_s, float step_ev, int* out_count) {
    *out_count = 0;
    // 注意：快门 max_s (例如 1s) > min_s (例如 1/1000s)
    if (max_s <= 0 || min_s <= 0 || max_s < min_s || step_ev <= 0) return NULL;

    const float* src_table = NULL;
    int src_total_count = 0;
    int stride = 1;

    // 智能选择母表和步长
    if (fabsf(step_ev - 0.5f) < 0.1f) {
        src_table = SHUTTERS_1_2;
        src_total_count = COUNT_SHUTTERS_1_2;
        stride = 1;
    } else if (fabsf(step_ev - 1.0f) < 0.1f) {
        src_table = SHUTTERS_1_3;
        src_total_count = COUNT_SHUTTERS_1_3;
        stride = 3; // 整档
    } else {
        src_table = SHUTTERS_1_3;
        src_total_count = COUNT_SHUTTERS_1_3;
        stride = 1;
    }

    // 表是降序的 (30s -> 1/8000s)
    
    // 1. 找起点：第一个 <= max_s (例如 1.0s)
    int start = 0;
    while (start < src_total_count && src_table[start] > max_s + 0.0001f) start++;

    // 2. 找终点：第一个 >= min_s (例如 1/1000s)
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

float mapping_aperture(float calculated_f, float* f_stop, int f_stop_count) {
    if (!f_stop || f_stop_count <= 0 || calculated_f <= 0) return -1.0f;

    // 遍历寻找最近的值
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

float mapping_shutter(float calculated_s, const float* s_stop, int s_stop_count) {
    if (!s_stop || s_stop_count <= 0 || calculated_s <= 0.0f) return -1.0f;

    // 遍历寻找最近的值
    // 技巧：由于快门范围跨度大 (30s 到 0.0001s)，直接减法在小数值时可能权重不够。
    // 严谨做法是比较 log2 的差值 (EV差)，但在这种离散表中，直接比较绝对差通常也足够。
    // 如果想更精确，可以使用比率：fabs(a/b - 1.0)
    
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

LEN add_Len_message(char* name, float f_max, float f_min, float aperture_step, float focal_length) {
    LEN len = {0};
    snprintf(len.name, sizeof(len.name), "%s", name);
    len.focal_length = focal_length;
    len.aperture_stops = generate_aperture(f_max, f_min, aperture_step, &len.aperture_stop_count);
    return len;
}

CAM add_Cam_message(char* name, float s_max, float s_min, float shutter_step, float flash_sync_shutter) {
    CAM cam = {0};
    snprintf(cam.name, sizeof(cam.name), "%s", name);
    cam.flash_sync_shutter = flash_sync_shutter;
    cam.shutter_stops = generate_shutter(s_max, s_min, shutter_step, &cam.shutter_stop_count);
    return cam;
}

void exposure_auto(float lux, float iso, bool in_hand, uint8_t auto_mode, LEN len, CAM cam, float* aperture, float* shutter) {
    if (!aperture || !shutter || lux <= 0 || iso <= 0) {
        *aperture = -1.0f; *shutter = -1.0f; return;
    }

    // 1. 获取硬件物理极限
    float len_f_max = (len.aperture_stops && len.aperture_stop_count > 0) ? len.aperture_stops[0] : 1.4f;
    float len_f_min = (len.aperture_stops && len.aperture_stop_count > 0) ? len.aperture_stops[len.aperture_stop_count - 1] : 22.0f;
    
    // 健壮地获取相机极限 (支持乱序表)
    float cam_s_fastest = 0.001f; 
    float cam_s_slowest = 1.0f;   
    if (cam.shutter_stops && cam.shutter_stop_count > 0) {
        float v1 = cam.shutter_stops[0];
        float v2 = cam.shutter_stops[cam.shutter_stop_count - 1];
        if (v1 < v2) { cam_s_fastest = v1; cam_s_slowest = v2; } // 升序
        else         { cam_s_fastest = v2; cam_s_slowest = v1; } // 降序 (标准情况)
    }

    // 2. 安全快门
    float safe_shutter = 1.0f / fmaxf(len.focal_length, 50.0f);
    if (!in_hand) safe_shutter = 30.0f;

    // 3. 初始策略
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

    // 物理限制截断
    if (target_f < len_f_max) target_f = len_f_max;
    if (target_f > len_f_min) target_f = len_f_min;

    // 4. 计算理论快门
    float target_s = exposure_aperture_priority(lux, iso, target_f);

    // 5. 约束求解
    // 情况 A: 太亮，超过最快快门
    if (target_s < cam_s_fastest) {
        float new_f = exposure_shutter_priority(lux, iso, cam_s_fastest);
        if (new_f > len_f_min) {
            target_f = len_f_min;
            target_s = cam_s_fastest; // 只能过曝
        } else {
            target_f = new_f;
            target_s = cam_s_fastest;
        }
    }
    // 情况 B: 太暗且手持
    else if (in_hand && target_s > safe_shutter) {
        if (target_f > len_f_max) {
             float s_at_max = exposure_aperture_priority(lux, iso, len_f_max);
             if (s_at_max <= safe_shutter) {
                 target_f = exposure_shutter_priority(lux, iso, safe_shutter);
                 if (target_f < len_f_max) target_f = len_f_max;
                 target_s = safe_shutter;
             } else {
                 target_f = len_f_max;
                 target_s = s_at_max; // 接受慢快门
             }
        }
    }
    // 情况 C: 超过相机最慢快门 (比如算出来 5秒，相机只有 1秒)
    if (target_s > cam_s_slowest) {
        // 1. 先把快门限制死在相机极限
        target_s = cam_s_slowest;

        // 2. 关键修复：基于这个极限快门，反推此时应该用什么光圈
        // 使用 exposure_shutter_priority 重新计算 f 值
        float re_calculated_f = exposure_shutter_priority(lux, iso, target_s);

        // 3. 检查反推出来的光圈是否可用
        if (re_calculated_f > len_f_min) {
            // 就算用到最小光圈(f/16)配合最慢快门(1s)，画面还是太亮(极其罕见，除非对着太阳拍长曝光)
            target_f = len_f_min;
        } 
        else if (re_calculated_f < len_f_max) {
            // 如果算出来需要 f/1.0，但镜头最大只有 f/1.4
            // 这才是真正需要“救急”的时候，只能全开光圈，并接受欠曝
            target_f = len_f_max;
        } 
        else {
            // 正常情况：比如原本想要 f/11 + 1.2s
            // 现在限制为 1.0s，反推出来光圈大概是 f/10 左右，这才是正确结果
            target_f = re_calculated_f;
        }
    }

    // 6. 映射 (Mapping)
    *aperture = (len.aperture_stops) ? 
                mapping_aperture(target_f, len.aperture_stops, len.aperture_stop_count) : target_f;
    *shutter = (cam.shutter_stops) ? 
               mapping_shutter(target_s, cam.shutter_stops, cam.shutter_stop_count) : target_s;
}