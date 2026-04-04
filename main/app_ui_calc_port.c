#include "app_ui_calc_port.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"

static const char* TAG = "app_ui_calc_port";

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
 * @brief 将快门速度值设置到 UI 滚轮
 * @param roller LVGL 快门滚轮对象
 * @param shutter 快门速度 (秒)
 * @param cam 相机参数结构体，包含快门档位数组
 * @note 如果提供了 cam 参数，会先从 cam 中提取快门数组并映射到最接近的档位，然后更新滚轮显示
 */
void ui_calc_port_set_shutter_to_roller(lv_obj_t* roller, float shutter, CAM cam)
{
    if (!roller || shutter <= 0) {
        ESP_LOGW(TAG, "Invalid roller or shutter: roller=%p, shutter=%.3f", (void*)roller, shutter);
        return;
    }

    // 获取 Port 层动态生成的选项字符串
    char* options = ui_calc_port_get_shutter_options();

    if (!options) {
        ESP_LOGE(TAG, "No shutter options available");
        return;  // 没有选项，无法更新
    }

    ESP_LOGD(TAG, "Setting shutter to %.3f", shutter);

    // 获取当前滚轮的选项字符串，只在选项改变时才更新
    const char* current_options = lv_roller_get_options(roller);
    if (!current_options || strcmp(current_options, options) != 0) {
        // 选项字符串改变了，需要更新
        ESP_LOGD(TAG, "Updating roller options");
        lv_roller_set_options(roller, options, LV_ANIM_OFF);
    }

    // 如果提供了相机参数，查找最接近的档位
    if (cam.shutter_stops && cam.shutter_stop_count > 0) {
        float mapped = mapping_shutter(shutter, cam.shutter_stops, cam.shutter_stop_count);
        ESP_LOGD(TAG, "Mapped shutter from %.3f to %.3f", shutter, mapped);
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

    ESP_LOGD(TAG, "Looking for shutter string: %s", buf);

    char* copy = strdup(options);
    char* token = strtok(copy, "\n");
    int index = 0;
    int found = 0;

    while (token != NULL) {
        ESP_LOGD(TAG, "Checking option %d: %s", index, token);
        if (strcmp(token, buf) == 0) {
            ESP_LOGD(TAG, "Found shutter at index %d", index);
            lv_roller_set_selected(roller, index, LV_ANIM_ON);
            found = 1;
            free(copy);
            return;
        }
        token = strtok(NULL, "\n");
        index++;
    }

    ESP_LOGW(TAG, "Shutter value %s not found in options", buf);
    free(copy);
}

/**
 * @brief 将光圈值设置到 UI 滚轮
 * @param roller LVGL 光圈滚轮对象
 * @param aperture 光圈值
 * @param len 镜头参数结构体，包含光圈档位数组
 * @note 如果提供了 len 参数，会先从 len 中提取光圈数组并映射到最接近的档位，然后更新滚轮显示
 */
void ui_calc_port_set_aperture_to_roller(lv_obj_t* roller, float aperture, LEN len)
{
    if (!roller || aperture <= 0) {
        ESP_LOGW(TAG, "Invalid roller or aperture: roller=%p, aperture=%.1f", (void*)roller, aperture);
        return;
    }

    // 获取 Port 层动态生成的选项字符串
    char* options = ui_calc_port_get_aperture_options();

    if (!options) {
        ESP_LOGE(TAG, "No aperture options available");
        return;  // 没有选项，无法更新
    }

    ESP_LOGD(TAG, "Setting aperture to %.1f", aperture);

    // 获取当前滚轮的选项字符串，只在选项改变时才更新
    const char* current_options = lv_roller_get_options(roller);
    if (!current_options || strcmp(current_options, options) != 0) {
        ESP_LOGD(TAG, "Updating roller options");
        lv_roller_set_options(roller, options, LV_ANIM_OFF);
    }

    // 如果提供了镜头参数，查找最接近的档位
    if (len.aperture_stops && len.aperture_stop_count > 0) {
        float mapped = mapping_aperture(aperture, (float*)len.aperture_stops, len.aperture_stop_count);
        ESP_LOGD(TAG, "Mapped aperture from %.1f to %.1f", aperture, mapped);
        aperture = mapped;
    }

    // 将光圈值转换为字符串格式
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f", aperture);

    ESP_LOGD(TAG, "Looking for aperture string: %s", buf);

    char* copy = strdup(options);
    char* token = strtok(copy, "\n");
    int index = 0;
    int found = 0;

    while (token != NULL) {
        ESP_LOGD(TAG, "Checking option %d: %s", index, token);
        if (strcmp(token, buf) == 0) {
            ESP_LOGD(TAG, "Found aperture at index %d", index);
            lv_roller_set_selected(roller, index, LV_ANIM_ON);
            found = 1;
            free(copy);
            return;
        }
        token = strtok(NULL, "\n");
        index++;
    }

    ESP_LOGW(TAG, "Aperture value %s not found in options", buf);
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

    // ESP_LOGD(TAG, "Extract cam from card: %p", (void*)card);

    if (!card) {
        ESP_LOGW(TAG, "Card is NULL");
        return cam;
    }

    // 获取卡片内容容器
    lv_obj_t *card_content = lv_obj_get_child(card, 0);
    // ESP_LOGD(TAG, "Card content: %p", (void*)card_content);
    if (!card_content) {
        ESP_LOGW(TAG, "Card content is NULL");
        return cam;
    }

    // 获取行容器
    lv_obj_t *row1 = lv_obj_get_child(card_content, 0);  // 第一行：名字和快门步进
    lv_obj_t *row2 = lv_obj_get_child(card_content, 1);  // 第二行：最小/最大快门和闪光同步

    // ESP_LOGD(TAG, "Rows: row1=%p, row2=%p", (void*)row1, (void*)row2);

    if (!row1 || !row2) {
        ESP_LOGW(TAG, "Row1 or Row2 is NULL");
        return cam;
    }

    // 从 row1 获取：名字 (索引 0), 快门步进 (索引 1)
    lv_obj_t *dropdown_shutter_step = lv_obj_get_child(row1, 1);

    // 从 row2 获取：最小快门 (索引 0), 最大快门 (索引 1), 闪光同步 (索引 2)
    lv_obj_t *dropdown_min_shutter = lv_obj_get_child(row2, 0);
    lv_obj_t *dropdown_max_shutter = lv_obj_get_child(row2, 1);
    lv_obj_t *textarea_flash_sync = lv_obj_get_child(row2, 2);

    // ESP_LOGD(TAG, "Dropdowns: step=%p, min=%p, max=%p, flash=%p",
    //          (void*)dropdown_shutter_step, (void*)dropdown_min_shutter,
    //          (void*)dropdown_max_shutter, (void*)textarea_flash_sync);

    if (dropdown_shutter_step && dropdown_min_shutter && dropdown_max_shutter) {
        uint32_t step_type = lv_dropdown_get_selected(dropdown_shutter_step);
        uint32_t min_idx = lv_dropdown_get_selected(dropdown_min_shutter);
        uint32_t max_idx = lv_dropdown_get_selected(dropdown_max_shutter);

        // ESP_LOGD(TAG, "Step type: %d, Min idx: %d, Max idx: %d", step_type, min_idx, max_idx);

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
                ESP_LOGD(TAG, "Extracted %d shutter stops, first=%.3f, last=%.3f",
                         cam.shutter_stop_count, cam.shutter_stops[0], cam.shutter_stops[cam.shutter_stop_count-1]);
            } else {
                ESP_LOGE(TAG, "Failed to allocate shutter stops memory");
            }
        } else {
            ESP_LOGW(TAG, "Actual count is 0");
        }
    } else {
        ESP_LOGW(TAG, "One or more dropdowns are NULL");
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

    // ESP_LOGD(TAG, "Extract len from card: %p", (void*)card);

    if (!card) {
        ESP_LOGW(TAG, "Card is NULL");
        return len;
    }

    // 获取卡片内容容器
    lv_obj_t *card_content = lv_obj_get_child(card, 0);
    // ESP_LOGD(TAG, "Card content: %p", (void*)card_content);
    if (!card_content) {
        ESP_LOGW(TAG, "Card content is NULL");
        return len;
    }

    // 获取行容器
    lv_obj_t *row1 = lv_obj_get_child(card_content, 0);  // 第一行：名字和光圈步进
    lv_obj_t *row2 = lv_obj_get_child(card_content, 1);  // 第二行：最小/最大光圈和焦距

    // ESP_LOGD(TAG, "Rows: row1=%p, row2=%p", (void*)row1, (void*)row2);

    if (!row1 || !row2) {
        ESP_LOGW(TAG, "Row1 or Row2 is NULL");
        return len;
    }

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

    // 从 row1 获取：名字 (索引 0), 光圈步进 (索引 1)
    lv_obj_t *dropdown_aperture_step = lv_obj_get_child(row1, 1);

    // 从 row2 获取：最小光圈 (索引 0), 最大光圈 (索引 1), 焦距 (索引 2)
    lv_obj_t *dropdown_min_aperture = lv_obj_get_child(row2, 0);
    lv_obj_t *dropdown_max_aperture = lv_obj_get_child(row2, 1);
    lv_obj_t *textarea_focal_length = lv_obj_get_child(row2, 2);

    // ESP_LOGD(TAG, "Dropdowns: step=%p, min=%p, max=%p, focal=%p",
    //          (void*)dropdown_aperture_step, (void*)dropdown_min_aperture,
    //          (void*)dropdown_max_aperture, (void*)textarea_focal_length);

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



ui_calc_data_t ui_calc_port_exposure(uint32_t lux, int iso, float ev, uint8_t mode,
                                        CAM cam, LEN len,
                                        lv_obj_t* roller_shutter, lv_obj_t* roller_aperture)
{
    ui_calc_data_t calc_data;
    calc_data.shutter = 0.0f;
    calc_data.aperture = 0.0f;
    memset(&calc_data.flags, 0, sizeof(calc_data.flags));

    if (mode != EXPOSURE_MANUAL)
    {
        exposure_auto(lux, iso, mode, len, cam, ev, &calc_data.aperture, &calc_data.shutter, &calc_data.flags);
    }
    else
    {
        ManualWheelType manual_mode = ui_calc_port_get_manual_wheel_type();

        // 获取相机和镜头的边界值
        float min_shutter = 0.001f;
        float max_shutter = 30.0f;
        float min_aperture = 1.4f;    // 最小 f 值 = 最大光圈
        float max_aperture = 22.0f;   // 最大 f 值 = 最小光圈

        if (cam.shutter_stops && cam.shutter_stop_count > 0) {
            // 快门：数值越小表示越快
            float v1 = cam.shutter_stops[0];
            float v2 = cam.shutter_stops[cam.shutter_stop_count - 1];
            min_shutter = (v1 < v2) ? v1 : v2;  // 最小值 = 最快快门
            max_shutter = (v1 > v2) ? v1 : v2;  // 最大值 = 最慢慢门
        }

        if (len.aperture_stops && len.aperture_stop_count > 0) {
            // 光圈：数值越小表示光圈越大
            // 数组通常是 [1.4, 2.0, 2.8, ..., 16, 22]，从小到大排列
            min_aperture = len.aperture_stops[0];              // 最小 f 值 = 最大光圈
            max_aperture = len.aperture_stops[len.aperture_stop_count - 1];  // 最大 f 值 = 最小光圈
        }

        if (manual_mode == MANUAL_WHEEL_TV)
        {
            calc_data.shutter = ui_calc_port_get_shutter_from_roller(roller_shutter);
            float calc_aperture = exposure_shutter_priority(lux, iso, ev, calc_data.shutter);

            ESP_LOGD(TAG, "TV mode: lux=%u, iso=%d, ev=%.1f, shutter=%.3f, calc aperture=%.1f",
                     lux, iso, ev, calc_data.shutter, calc_aperture);

            if (calc_aperture > 0 && len.aperture_stops && len.aperture_stop_count > 0) {
                float actual_min_aperture = len.aperture_stops[0];
                float actual_max_aperture = len.aperture_stops[len.aperture_stop_count - 1];

                calc_data.aperture = mapping_aperture(calc_aperture, len.aperture_stops, len.aperture_stop_count);

                ESP_LOGD(TAG, "Aperture: bounds [%.1f, %.1f], calc=%.1f, mapped=%.1f",
                         actual_min_aperture, actual_max_aperture, calc_aperture, calc_data.aperture);

                if (calc_aperture < actual_min_aperture) {
                    calc_data.aperture = actual_min_aperture;
                    calc_data.flags.aperture_out_of_range = 1;
                    calc_data.flags.overexposure = 1;
                    ESP_LOGW(TAG, "Aperture too large (need f/%.1f < min f/%.1f), clamped to f/%.1f",
                             calc_aperture, actual_min_aperture, actual_min_aperture);
                } else if (calc_aperture > actual_max_aperture) {
                    calc_data.aperture = actual_max_aperture;
                    calc_data.flags.aperture_out_of_range = 1;
                    calc_data.flags.underexposure = 1;
                    ESP_LOGW(TAG, "Aperture too small (need f/%.1f > max f/%.1f), clamped to f/%.1f",
                             calc_aperture, actual_max_aperture, actual_max_aperture);
                }
            } else {
                calc_data.aperture = calc_aperture;
            }
        }
        else if (manual_mode == MANUAL_WHEEL_AV)
        {
            calc_data.aperture = ui_calc_port_get_aperture_from_roller(roller_aperture);
            float calc_shutter = exposure_aperture_priority(lux, iso, ev, calc_data.aperture);

            ESP_LOGD(TAG, "AV mode: lux=%u, iso=%d, ev=%.1f, aperture=%.1f, calc shutter=%.3f",
                     lux, iso, ev, calc_data.aperture, calc_shutter);

            if (calc_shutter > 0 && cam.shutter_stops && cam.shutter_stop_count > 0) {
                float actual_min_shutter = min_shutter;
                float actual_max_shutter = max_shutter;

                calc_data.shutter = mapping_shutter(calc_shutter, cam.shutter_stops, cam.shutter_stop_count);

                ESP_LOGD(TAG, "Shutter: bounds [%.3f, %.3f], calc=%.3f, mapped=%.3f",
                         actual_min_shutter, actual_max_shutter, calc_shutter, calc_data.shutter);

                if (calc_shutter < actual_min_shutter) {
                    calc_data.shutter = actual_min_shutter;
                    calc_data.flags.shutter_out_of_range = 1;
                    calc_data.flags.overexposure = 1;
                    ESP_LOGW(TAG, "Shutter too fast (need %.3f < min %.3f), clamped to %.3f",
                             calc_shutter, actual_min_shutter, actual_min_shutter);
                } else if (calc_shutter > actual_max_shutter) {
                    calc_data.shutter = actual_max_shutter;
                    calc_data.flags.shutter_out_of_range = 1;
                    calc_data.flags.underexposure = 1;
                    ESP_LOGW(TAG, "Shutter too slow (need %.3f > max %.3f), clamped to %.3f",
                             calc_shutter, actual_max_shutter, actual_max_shutter);
                }
            } else {
                calc_data.shutter = calc_shutter;
            }

            float safe_shutter = 1.0f / fmaxf(len.focal_length, 50.0f);
            if (calc_data.shutter > safe_shutter) {
                calc_data.flags.slow_shutter_warning = 1;
            }
        }
        else
        {
            // MANUAL_WHEEL_NONE - 手动模式但未操作滚轮，保持当前值
            calc_data.shutter = ui_calc_port_get_shutter_from_roller(roller_shutter);
            calc_data.aperture = ui_calc_port_get_aperture_from_roller(roller_aperture);
        }
    }

    return calc_data;
}

// ──────────────────────────────────────────────
// 曝光模式转换
// ──────────────────────────────────────────────
/**
 * @brief 获取当前曝光模式
 * @return EXPOSURE_MANUAL(0), EXPOSURE_AUTO(1), EXPOSURE_LANDSCAPE(2), EXPOSURE_PORTRAIT(3)
 * @note UI索引与EXPOSURE枚举的映射：
 *       UI索引0 -> EXPOSURE_AUTO (auto_mode)
 *       UI索引1 -> EXPOSURE_LANDSCAPE (landscape_mode)
 *       UI索引2 -> EXPOSURE_MANUAL (manual_mode)
 *       UI索引3 -> EXPOSURE_PORTRAIT (portrait_mode)
 */
uint8_t ui_calc_port_get_exposure_mode(void) {
    extern uint8_t app_ui_get_selected_mode(void);
    uint8_t ui_idx = app_ui_get_selected_mode();

    switch (ui_idx) {
        case 0: return EXPOSURE_AUTO;
        case 1: return EXPOSURE_LANDSCAPE;
        case 2: return EXPOSURE_MANUAL;
        case 3: return EXPOSURE_PORTRAIT;
        default: return EXPOSURE_MANUAL;
    }
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

// ──────────────────────────────────────────────
// 配置保存
// ──────────────────────────────────────────────

/**
 * @brief 保存配置到NVS
 * @note 保存相机、镜头等配置数据
 */
void ui_calc_port_save_config(void)
{
    extern int app_nvs_save_all(void);
    app_nvs_save_all();
}

// ──────────────────────────────────────────────
// 警告颜色显示
// ──────────────────────────────────────────────

void ui_calc_port_update_roller_warning_color(lv_obj_t* shutter_roller, lv_obj_t* aperture_roller, ExposureFlags flags)
{
    lv_color_t tv_color = lv_color_hex(0xFFFFFF);
    lv_color_t av_color = lv_color_hex(0xFFFFFF);

    if (flags.overexposure) {
        tv_color = lv_color_hex(0xFF8800);
        av_color = lv_color_hex(0xFF8800);
    } else if (flags.underexposure) {
        tv_color = lv_color_hex(0x4488FF);
        av_color = lv_color_hex(0x4488FF);
    } else {
        if (flags.shutter_out_of_range) {
            tv_color = lv_color_hex(0xFF4444);
        } else if (flags.slow_shutter_warning) {
            tv_color = lv_color_hex(0xFFCC00);
        }
        if (flags.aperture_out_of_range) {
            av_color = lv_color_hex(0xFF4444);
        }
    }

    lv_obj_set_style_text_color(shutter_roller, tv_color, LV_PART_SELECTED);
    lv_obj_set_style_text_color(aperture_roller, av_color, LV_PART_SELECTED);
}
