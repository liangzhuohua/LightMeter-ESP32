#include "app_ui_calc_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// ──────────────────────────────────────────────
// 辅助函数：解析快门字符串
// ──────────────────────────────────────────────
/**
 * @brief 解析快门字符串为浮点数值
 * @param str 快门字符串，支持格式："1/60"、"0.5"、"1"、"30" 等
 * @return 快门速度(秒)，解析失败返回-1.0f
 * @note 支持分数格式（如"1/60"）和小数格式（如"0.5"）
 */
static float parse_shutter_string(const char* str)
{
    if (!str || strlen(str) == 0) return -1.0f;
    
    // 处理分数格式 "1/60"
    if (strchr(str, '/') != NULL) {
        int numerator = 1;
        int denominator = 1;
        sscanf(str, "%d/%d", &numerator, &denominator);
        if (denominator > 0) {
            return (float)numerator / denominator;
        }
    }
    
    // 处理小数格式 "0.5" 或整数格式 "1"
    float value = atof(str);
    return value;
}

// ──────────────────────────────────────────────
// 辅助函数：解析光圈字符串
// ──────────────────────────────────────────────
/**
 * @brief 解析光圈字符串为浮点数值
 * @param str 光圈字符串，如 "1.4"、"2.8"、"4.0" 等
 * @return 光圈值，解析失败返回-1.0f
 * @note 光圈值通常为小数，如 f/1.4、f/2.8
 */
static float parse_aperture_string(const char* str)
{
    if (!str || strlen(str) == 0) return -1.0f;
    return atof(str);
}

// ──────────────────────────────────────────────
// 辅助函数：解析ISO字符串
// ──────────────────────────────────────────────
/**
 * @brief 解析ISO字符串为整数值
 * @param str ISO字符串，如 "100"、"200"、"800" 等
 * @return ISO值，解析失败返回-1
 * @note ISO值为整数，通常为 50、100、200、400、800、1600 等
 */
static int parse_iso_string(const char* str)
{
    if (!str || strlen(str) == 0) return -1;
    return atoi(str);
}

// ──────────────────────────────────────────────
// 辅助函数：解析EV字符串
// ──────────────────────────────────────────────
/**
 * @brief 解析曝光补偿(EV)字符串为浮点数值
 * @param str EV字符串，支持格式："+1"、"-1"、"0"、"+1/3"、"-1/3" 等
 * @return EV值，解析失败返回0.0f
 * @note EV值可以为正数（增加曝光）、负数（减少曝光）或零
 */
static float parse_ev_string(const char* str)
{
    if (!str || strlen(str) == 0) return 0.0f;
    
    // 处理 "+1/3", "-1/3" 格式
    if (strchr(str, '/') != NULL) {
        int numerator = 0;
        int denominator = 1;
        sscanf(str, "%d/%d", &numerator, &denominator);
        if (denominator > 0) {
            return (float)numerator / denominator;
        }
    }
    
    // 处理 "+1", "-1", "0" 格式
    return atof(str);
}

// ──────────────────────────────────────────────
// 初始化与清理
// ──────────────────────────────────────────────
/**
 * @brief 初始化UI计算Port层
 * @note 在程序启动时调用，初始化Port层所需的资源
 */
void ui_calc_port_init(void)
{
    // 初始化Port层资源
}

/**
 * @brief 清理UI计算Port层资源
 * @note 在程序退出时调用，释放Port层占用的资源
 */
void ui_calc_port_deinit(void)
{
    // 清理Port层资源
}

// ──────────────────────────────────────────────
// 数据转换：UI -> 算法
// ──────────────────────────────────────────────
/**
 * @brief 从UI快门滚轮获取快门速度值
 * @param roller LVGL快门滚轮对象
 * @return 快门速度(秒)，失败返回-1.0f
 * @note 从滚轮获取选中的字符串，并解析为浮点数值
 */
float ui_calc_port_get_shutter_from_roller(lv_obj_t* roller)
{
    if (!roller) return -1.0f;
    
    char buf[32];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    return parse_shutter_string(buf);
}

/**
 * @brief 从UI光圈滚轮获取光圈值
 * @param roller LVGL光圈滚轮对象
 * @return 光圈值，失败返回-1.0f
 * @note 从滚轮获取选中的字符串，并解析为浮点数值
 */
float ui_calc_port_get_aperture_from_roller(lv_obj_t* roller)
{
    if (!roller) return -1.0f;
    
    char buf[32];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    return parse_aperture_string(buf);
}

/**
 * @brief 从UI ISO滚轮获取ISO值
 * @param roller LVGL ISO滚轮对象
 * @return ISO值，失败返回-1
 * @note 从滚轮获取选中的字符串，并解析为整数值
 */
int ui_calc_port_get_iso_from_roller(lv_obj_t* roller)
{
    if (!roller) return -1;
    
    char buf[32];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    return parse_iso_string(buf);
}

/**
 * @brief 从UI EV滚轮获取曝光补偿值
 * @param roller LVGL EV滚轮对象
 * @return EV值，失败返回0.0f
 * @note 从滚轮获取选中的字符串，并解析为浮点数值
 */
float ui_calc_port_get_ev_from_roller(lv_obj_t* roller)
{
    if (!roller) return 0.0f;
    
    char buf[32];
    lv_roller_get_selected_str(roller, buf, sizeof(buf));
    return parse_ev_string(buf);
}

/**
 * @brief 从UI Lux标签获取环境照度值
 * @param label LVGL Lux标签对象
 * @return Lux值，失败返回-1
 * @note 从标签获取文本内容，并解析为整数值
 */
int ui_calc_port_get_lux_from_label(lv_obj_t* label)
{
    if (!label) return -1;
    
    const char* text = lv_label_get_text(label);
    if (!text) return -1;
    
    return atoi(text);
}

// ──────────────────────────────────────────────
// 数据转换：算法 -> UI
// ──────────────────────────────────────────────
/**
 * @brief 将快门速度值设置到UI滚轮
 * @param roller LVGL快门滚轮对象
 * @param shutter 快门速度(秒)
 * @param shutter_array 可选的快门档位数组，用于查找最接近的档位
 * @param count 快门档位数组数量
 * @note 如果提供了快门数组，会先映射到最接近的档位，然后更新滚轮显示
 */
void ui_calc_port_set_shutter_to_roller(lv_obj_t* roller, float shutter,
                                         const float* shutter_array, int count)
{
    if (!roller || shutter <= 0) return;
    
    // 获取 Port 层动态生成的选项字符串
    char* options = ui_calc_port_get_shutter_options();
    
    if (!options) {
        return;  // 没有选项，无法更新
    }
    
    // 获取当前滚轮的选项字符串，只在选项改变时才更新
    const char* current_options = lv_roller_get_options(roller);
    if (!current_options || strcmp(current_options, options) != 0) {
        // 选项字符串改变了，需要更新
        lv_roller_set_options(roller, options, LV_ANIM_OFF);
    }
    
    // 如果提供了快门数组，查找最接近的档位
    if (shutter_array && count > 0) {
        float mapped = mapping_shutter(shutter, shutter_array, count);
        shutter = mapped;
    }
    
    // 将快门值转换为字符串格式
    char buf[32];
    if (shutter >= 1.0f) {
        snprintf(buf, sizeof(buf), "%.0f\"", shutter);
    } else {
        float denom = 1.0f / shutter;
        snprintf(buf, sizeof(buf), "1/%d", (int)roundf(denom));
    }
    
    // 使用字符串分割查找索引
    char* copy = strdup(options);
    char* token = strtok(copy, "\n");
    int index = 0;

    while (token != NULL) {
        if (strstr(token, buf) != NULL) {
            lv_roller_set_selected(roller, index, LV_ANIM_OFF);
            free(copy);
            return;
        }
        token = strtok(NULL, "\n");
        index++;
    }

    free(copy);
}

/**
 * @brief 将光圈值设置到UI滚轮
 * @param roller LVGL光圈滚轮对象
 * @param aperture 光圈值
 * @param aperture_array 可选的光圈档位数组，用于查找最接近的档位
 * @param count 光圈档位数组数量
 * @note 如果提供了光圈数组，会先映射到最接近的档位，然后更新滚轮显示
 */
void ui_calc_port_set_aperture_to_roller(lv_obj_t* roller, float aperture,
                                          const float* aperture_array, int count)
{
    if (!roller || aperture <= 0) return;
    
    // 获取 Port 层动态生成的选项字符串
    char* options = ui_calc_port_get_aperture_options();
    
    if (!options) {
        return;  // 没有选项，无法更新
    }
    
    // 获取当前滚轮的选项字符串，只在选项改变时才更新
    const char* current_options = lv_roller_get_options(roller);
    if (!current_options || strcmp(current_options, options) != 0) {
        // 选项字符串改变了，需要更新
        lv_roller_set_options(roller, options, LV_ANIM_OFF);
    }
    
    // 如果提供了光圈数组，查找最接近的档位
    if (aperture_array && count > 0) {
        float mapped = mapping_aperture(aperture, (float*)aperture_array, count);
        aperture = mapped;
    }
    
    // 将光圈值转换为字符串格式
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", aperture);
    
    // 使用字符串分割查找索引
    char* copy = strdup(options);
    char* token = strtok(copy, "\n");
    int index = 0;

    while (token != NULL) {
        if (strstr(token, buf) != NULL) {
            lv_roller_set_selected(roller, index, LV_ANIM_OFF);
            free(copy);
            return;
        }
        token = strtok(NULL, "\n");
        index++;
    }

    free(copy);
}

/**
 * @brief 更新UI上的Lux标签显示
 * @param label LVGL Lux标签对象
 * @param lux 环境照度值
 * @note 将Lux值格式化为字符串并设置到标签
 */
void ui_calc_port_update_lux_label(lv_obj_t* label, int lux)
{
    if (!label) return;
    lv_label_set_text_fmt(label, "%d", lux);
}

// ──────────────────────────────────────────────
// 相机/镜头数据提取
// ──────────────────────────────────────────────
/**
 * @brief 从相机卡片UI对象中提取相机参数
 * @param card 相机卡片LVGL对象
 * @return CAM结构体，包含相机参数
 * @note 调用者需要负责释放返回的CAM结构体中的shutter_stops内存
 * @warning 此函数假设相机卡片的结构与app_ui.c中的定义一致
 */
static float parse_flash_sync_value(const char* str)
{
    if (!str || strlen(str) == 0) {
        return 1.0f / 250.0f;
    }
    
    const char* slash = strchr(str, '/');
    if (slash != NULL) {
        float numerator = atof(str);
        float denominator = atof(slash + 1);
        if (denominator > 0) {
            return numerator / denominator;
        }
    }
    
    return atof(str);
}

CAM ui_calc_port_extract_cam_from_card(lv_obj_t* card)
{
    CAM cam = {0};
    cam.shutter_stops = NULL;
    cam.shutter_stop_count = 0;
    cam.flash_sync_shutter = 0.0f;  // 0 表示无闪光同步限制
    
    if (!card) return cam;
    
    // 获取卡片内容容器
    lv_obj_t *card_content = lv_obj_get_child(card, 0);
    if (!card_content) return cam;
    
    // 获取下拉菜单：快门步进、最小快门、最大快门
    lv_obj_t *dropdown_shutter_step = lv_obj_get_child(card_content, 1);
    lv_obj_t *dropdown_min_shutter = lv_obj_get_child(card_content, 2);
    lv_obj_t *dropdown_max_shutter = lv_obj_get_child(card_content, 3);
    lv_obj_t *textarea_flash_sync = lv_obj_get_child(card_content, 4);
    
    if (dropdown_shutter_step && dropdown_min_shutter && dropdown_max_shutter) {
        uint32_t step_type = lv_dropdown_get_selected(dropdown_shutter_step);
        uint32_t min_idx = lv_dropdown_get_selected(dropdown_min_shutter);
        uint32_t max_idx = lv_dropdown_get_selected(dropdown_max_shutter);
        
        const float *shutter_array;
        int count;
        int stride = 1;
        
        // 根据步进类型选择快门数组
        if (step_type == 0) {
            shutter_array = SHUTTERS_1_3;
            count = COUNT_SHUTTERS_1_3;
            stride = 3;
        } else if (step_type == 1) {
            shutter_array = SHUTTERS_1_2;
            count = COUNT_SHUTTERS_1_2;
            stride = 1;
        } else {
            shutter_array = SHUTTERS_1_3;
            count = COUNT_SHUTTERS_1_3;
            stride = 1;
        }
        
        // 计算原始数组中的索引
        int orig_min_idx = (int)min_idx * stride;
        int orig_max_idx = (int)max_idx * stride;
        
        // 计算实际需要的快门数量
        int actual_count = 0;
        for (int i = orig_min_idx; i >= orig_max_idx && i >= 0 && i < count; i -= stride) {
            actual_count++;
        }
        
        // 分配内存并复制快门值
        if (actual_count > 0) {
            cam.shutter_stops = (float*)malloc(actual_count * sizeof(float));
            if (cam.shutter_stops) {
                cam.shutter_stop_count = actual_count;
                int idx = 0;
                for (int i = orig_min_idx; i >= orig_max_idx && i >= 0 && i < count; i -= stride) {
                    cam.shutter_stops[idx++] = shutter_array[i];
                }
            }
        }
    }
    
    // 读取闪光同步值
    if (textarea_flash_sync) {
        const char* flash_sync_str = lv_textarea_get_text(textarea_flash_sync);
        if (flash_sync_str && strlen(flash_sync_str) > 0) {
            cam.flash_sync_shutter = parse_flash_sync_value(flash_sync_str);
        }
    }
    
    return cam;
}

/**
 * @brief 从镜头卡片UI对象中提取镜头参数
 * @param card 镜头卡片LVGL对象
 * @return LEN结构体，包含镜头参数
 * @note 调用者需要负责释放返回的LEN结构体中的aperture_stops内存
 * @warning 此函数假设镜头卡片的结构与app_ui.c中的定义一致
 */
static float parse_focal_length_value(const char* str)
{
    if (!str || strlen(str) == 0) {
        return 50.0f;
    }
    
    return atof(str);
}

LEN ui_calc_port_extract_len_from_card(lv_obj_t* card)
{
    LEN len = {0};
    len.aperture_stops = NULL;
    len.aperture_stop_count = 0;
    len.focal_length = 0.0f;  // 0 表示无自定义焦距
    
    if (!card) return len;
    
    // 获取卡片内容容器
    lv_obj_t *card_content = lv_obj_get_child(card, 0);
    if (!card_content) return len;
    
    // 获取镜头卡片参数（存储在用户数据中）
    typedef struct {
        lv_obj_t *dropdown_aperture_step;
        lv_obj_t *dropdown_min_aperture;
        lv_obj_t *dropdown_max_aperture;
        lv_obj_t *textarea_custom_aperture;
        lv_obj_t *textarea_focal_length;
        float *custom_aperture_array;
        int custom_aperture_count;
        int current_step_type;
    } len_card_params_t;
    
    len_card_params_t *params = (len_card_params_t *)lv_obj_get_user_data(card);
    
    // 获取下拉菜单：光圈步进、最小光圈、最大光圈
    lv_obj_t *dropdown_aperture_step = lv_obj_get_child(card_content, 1);
    lv_obj_t *dropdown_min_aperture = lv_obj_get_child(card_content, 2);
    lv_obj_t *dropdown_max_aperture = lv_obj_get_child(card_content, 3);
    lv_obj_t *textarea_focal_length = lv_obj_get_child(card_content, 5);
    
    if (dropdown_aperture_step) {
        uint32_t step_type = lv_dropdown_get_selected(dropdown_aperture_step);
        
        // 处理自定义光圈数组
        if (step_type == 3 && params && params->custom_aperture_array && params->custom_aperture_count > 0) {
            len.aperture_stop_count = params->custom_aperture_count;
            len.aperture_stops = (float*)malloc(params->custom_aperture_count * sizeof(float));
            if (len.aperture_stops) {
                memcpy(len.aperture_stops, params->custom_aperture_array,
                       params->custom_aperture_count * sizeof(float));
            }
            return len;
        }
        
        // 处理标准光圈数组
        if (dropdown_min_aperture && dropdown_max_aperture) {
            uint32_t min_idx = lv_dropdown_get_selected(dropdown_min_aperture);
            uint32_t max_idx = lv_dropdown_get_selected(dropdown_max_aperture);
            
            const float *aperture_array;
            int count;
            int stride = 1;
            
            // 根据步进类型选择光圈数组
            if (step_type == 0) {
                aperture_array = APERTURES_1_3;
                count = COUNT_APERTURES_1_3;
                stride = 3;
            } else if (step_type == 1) {
                aperture_array = APERTURES_1_2;
                count = COUNT_APERTURES_1_2;
                stride = 1;
            } else {
                aperture_array = APERTURES_1_3;
                count = COUNT_APERTURES_1_3;
                stride = 1;
            }
            
            // 计算原始数组中的索引
            int orig_min_idx = (int)min_idx * stride;
            int orig_max_idx = (int)max_idx * stride;
            
            // 计算实际需要的光圈数量
            int actual_count = 0;
            for (int i = orig_min_idx; i <= orig_max_idx && i >= 0 && i < count; i += stride) {
                actual_count++;
            }
            
            // 分配内存并复制光圈值
            if (actual_count > 0) {
                len.aperture_stops = (float*)malloc(actual_count * sizeof(float));
                if (len.aperture_stops) {
                    len.aperture_stop_count = actual_count;
                    int idx = 0;
                    for (int i = orig_min_idx; i <= orig_max_idx && i >= 0 && i < count; i += stride) {
                        len.aperture_stops[idx++] = aperture_array[i];
                    }
                }
            }
        }
    }
    
    // 读取焦距值
    if (textarea_focal_length) {
        const char* focal_length_str = lv_textarea_get_text(textarea_focal_length);
        if (focal_length_str && strlen(focal_length_str) > 0) {
            len.focal_length = parse_focal_length_value(focal_length_str);
        }
    }
    
    return len;
}

// ──────────────────────────────────────────────
// 动态生成选项字符串
// ──────────────────────────────────────────────
/**
 * @brief 根据当前选中的相机卡片动态生成快门选项字符串
 * @return 快门选项字符串（使用\n分隔），如果没有选中卡片则返回NULL
 * @note 此函数会从cam_selected_card中提取快门数组并生成选项字符串
 */
char* ui_calc_port_get_shutter_options(void)
{
    extern lv_obj_t* app_ui_get_cam_selected_card(void);
    lv_obj_t* cam_card = app_ui_get_cam_selected_card();
    
    if (!cam_card) {
        return NULL;
    }

    // 从相机卡片提取参数
    CAM cam = ui_calc_port_extract_cam_from_card(cam_card);
    
    if (!cam.shutter_stops || cam.shutter_stop_count <= 0) {
        return NULL;
    }

    // 调用UI层的辅助函数生成选项字符串
    extern char* app_ui_generate_shutter_options(const float* shutter_array, int count, int stride);
    char* options = app_ui_generate_shutter_options(
        cam.shutter_stops,
        cam.shutter_stop_count,
        1  // 步长为1，显示所有档位
    );

    // 释放相机参数内存
    free(cam.shutter_stops);
    
    return options;
}

/**
 * @brief 根据当前选中的镜头卡片动态生成光圈选项字符串
 * @return 光圈选项字符串（使用\n分隔），如果没有选中卡片则返回NULL
 * @note 此函数会从len_selected_card中提取光圈数组并生成选项字符串
 */
char* ui_calc_port_get_aperture_options(void)
{
    extern lv_obj_t* app_ui_get_len_selected_card(void);
    lv_obj_t* len_card = app_ui_get_len_selected_card();
    
    if (!len_card) {
        return NULL;
    }

    // 从镜头卡片提取参数
    LEN len = ui_calc_port_extract_len_from_card(len_card);
    
    if (!len.aperture_stops || len.aperture_stop_count <= 0) {
        return NULL;
    }

    // 调用UI层的辅助函数生成选项字符串
    extern char* app_ui_generate_aperture_options(const float* aperture_array, int count, int stride);
    char* options = app_ui_generate_aperture_options(
        len.aperture_stops,
        len.aperture_stop_count,
        1  // 步长为1，显示所有档位
    );

    // 释放镜头参数内存
    free(len.aperture_stops);
    
    return options;
}

// ──────────────────────────────────────────────
// 曝光计算（整合UI和算法）
// ──────────────────────────────────────────────
/**
 * @brief 执行曝光计算并更新UI显示
 * @param lux 环境照度值
 * @param iso ISO感光度值
 * @param ev 曝光补偿值
 * @param mode 拍摄模式：EXPOSURE_AUTO(全自动)、EXPOSURE_LANDSCAPE(风光)、EXPOSURE_PORTRAIT(人像)
 * @param cam 相机参数结构体
 * @param len 镜头参数结构体
 * @param roller_shutter 快门滚轮对象（用于更新显示）
 * @param roller_aperture 光圈滚轮对象（用于更新显示）
 * @return ui_calc_data_t结构体，包含计算结果和状态标志
 * @note 根据模式自动选择计算方式，并将结果更新到UI滚轮
 */
ui_calc_data_t ui_calc_port_calculate_exposure(float lux, int iso, float ev, uint8_t mode,
                                                CAM cam, LEN len,
                                                lv_obj_t* roller_shutter, lv_obj_t* roller_aperture)
{
    ui_calc_data_t result = {0};
    result.lux = lux;
    result.iso = (float)iso;
    result.ev = ev;
    result.mode = mode;
    result.shutter = -1.0f;
    result.aperture = -1.0f;
    
    if (lux <= 0 || iso <= 0) {
        return result;
    }
    
    // 根据模式选择计算方式
    switch (mode) {
        case EXPOSURE_AUTO:
        case EXPOSURE_LANDSCAPE:
        case EXPOSURE_PORTRAIT: {
            // 自动曝光模式
            exposure_auto(lux, (float)iso, mode, len, cam, &result.aperture, &result.shutter, &result.flags);
            break;
        }
        default: {
            // 手动模式或其他模式
            // 这里可以添加其他计算逻辑
            break;
        }
    }
    
    // 更新UI
    if (roller_shutter && result.shutter > 0) {
        ui_calc_port_set_shutter_to_roller(roller_shutter, result.shutter,
                                             cam.shutter_stops, cam.shutter_stop_count);
    }
    
    if (roller_aperture && result.aperture > 0) {
        ui_calc_port_set_aperture_to_roller(roller_aperture, result.aperture,
                                            len.aperture_stops, len.aperture_stop_count);
    }
    
    return result;
}

/**
 * @brief 从当前UI参数执行曝光计算
 * @param roller_shutter 快门滚轮对象
 * @param roller_aperture 光圈滚轮对象
 * @param roller_iso ISO滚轮对象
 * @param roller_ev EV滚轮对象
 * @param label_lux Lux标签对象
 * @param mode 拍摄模式
 * @param cam 相机参数结构体
 * @param len 镜头参数结构体
 * @return ui_calc_data_t结构体，包含计算结果和状态标志
 * @note 从UI滚轮和标签中获取当前参数，然后调用曝光计算函数
 */
ui_calc_data_t ui_calc_port_calculate_from_ui(lv_obj_t* roller_shutter, lv_obj_t* roller_aperture,
                                               lv_obj_t* roller_iso, lv_obj_t* roller_ev,
                                               lv_obj_t* label_lux, uint8_t mode,
                                               CAM cam, LEN len)
{
    // 从UI获取参数
    int iso = ui_calc_port_get_iso_from_roller(roller_iso);
    float ev = ui_calc_port_get_ev_from_roller(roller_ev);
    int lux = ui_calc_port_get_lux_from_label(label_lux);
    
    // 调用计算函数
    return ui_calc_port_calculate_exposure((float)lux, iso, ev, mode, cam, len,
                                             roller_shutter, roller_aperture);
}

// ──────────────────────────────────────────────
// 自动模式支持
// ──────────────────────────────────────────────
/**
 * @brief 自动曝光计算（全自动、风光、人像模式）
 * @param lux 环境照度值
 * @param iso ISO感光度值
 * @param mode 拍摄模式：EXPOSURE_AUTO(全自动)、EXPOSURE_LANDSCAPE(风光)、EXPOSURE_PORTRAIT(人像)
 * @param cam 相机参数结构体
 * @param len 镜头参数结构体
 * @param roller_shutter 快门滚轮对象（用于更新显示）
 * @param roller_aperture 光圈滚轮对象（用于更新显示）
 * @return ExposureFlags结构体，包含曝光状态标志
 * @note 自动计算最佳光圈和快门组合，并更新到UI滚轮
 */
ExposureFlags ui_calc_port_auto_exposure(float lux, int iso, uint8_t mode,
                                         CAM cam, LEN len,
                                         lv_obj_t* roller_shutter, lv_obj_t* roller_aperture)
{
    ExposureFlags flags = {0};
    float aperture = -1.0f;
    float shutter = -1.0f;
    
    if (lux <= 0 || iso <= 0) {
        return flags;
    }
    
    // 调用自动曝光计算
    exposure_auto(lux, (float)iso, mode, len, cam, &aperture, &shutter, &flags);
    
    // 更新UI
    if (roller_shutter && shutter > 0) {
        ui_calc_port_set_shutter_to_roller(roller_shutter, shutter,
                                             cam.shutter_stops, cam.shutter_stop_count);
    }
    
    if (roller_aperture && aperture > 0) {
        ui_calc_port_set_aperture_to_roller(roller_aperture, aperture,
                                            len.aperture_stops, len.aperture_stop_count);
    }
    
    return flags;
}


// ──────────────────────────────────────────────
// 手动模式滚轮标志位实现
// ──────────────────────────────────────────────
// 手动模式滚轮标志位
// ──────────────────────────────────────────────
static ManualWheelType manual_wheel_type = MANUAL_WHEEL_NONE;

/**
 * @brief 获取手动模式滚轮标志位
 * @return 当前滚轮操作类型
 *     - MANUAL_WHEEL_NONE: 无滚轮操作
 *     - MANUAL_WHEEL_TV: Tv(快门)滚轮被滑动
 *     - MANUAL_WHEEL_AV: Av(光圈)滚轮被滑动
 * @note 此标志位用于在手动模式下区分用户操作的是哪个滚轮
 *       只有当 mode == EXPOSURE_MANUAL 时此值才有意义
 *       在曝光计算完成后应调用 ui_calc_port_reset_manual_wheel_type() 重置
 */
ManualWheelType ui_calc_port_get_manual_wheel_type(void)
{
    return manual_wheel_type;
}

/**
 * @brief 设置手动模式滚轮标志位
 * @param type 滚轮操作类型 (MANUAL_WHEEL_TV / MANUAL_WHEEL_AV)
 * @note 由UI层调用，当用户滑动 Tv 或 Av 滚轮时设置此标志位
 *       该函数内部被 roller_manual_mode_event_cb 回调调用
 */
void ui_calc_port_set_manual_wheel_type(ManualWheelType type)
{
    manual_wheel_type = type;
}

/**
 * @brief 重置手动模式滚轮标志位为 NONE
 * @note 在每次曝光计算完成后调用，以清除之前的滚轮操作记录
 *       防止标志位残留影响后续判断
 */
void ui_calc_port_reset_manual_wheel_type(void)
{
    manual_wheel_type = MANUAL_WHEEL_NONE;
}
