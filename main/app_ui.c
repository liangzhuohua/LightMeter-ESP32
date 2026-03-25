#include "app_ui.h"
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "app_exposure_calc.h"
#include "app_ui_calc_port.h"

// ──────────────────────────────────────────────
// 自定义键盘映射（数字 + 逗号，用于自定义光圈输入）
// ──────────────────────────────────────────────
static const char* custom_aperture_kb_map[] = {
    "1", "2", "3", LV_SYMBOL_BACKSPACE, "\n",
    "4", "5", "6", ",", "\n",
    "7", "8", "9", ".", "\n",
    "0", LV_SYMBOL_OK, LV_SYMBOL_CLOSE, "", "\n",
    NULL
};

static const lv_btnmatrix_ctrl_t custom_aperture_kb_ctrl[] = {
    2, 2, 2, 4,
    2, 2, 2, 2,
    2, 2, 2, 2,
    2, 4, 2, 2,
    LV_BTNMATRIX_CTRL_HIDDEN
};

// ──────────────────────────────────────────────
// 屏幕尺寸宏定义
// ──────────────────────────────────────────────
#define scr_act_width()     lv_obj_get_width(lv_scr_act())              // 宽
#define scr_act_height()    lv_obj_get_height(lv_scr_act())             // 高

// ──────────────────────────────────────────────
// 字体与常量
// ──────────────────────────────────────────────
static const lv_font_t* font;                                           // 字体

static const char* roller_shutter_options = "1\n1/2\n1/4\n1/8\n1/15\n1/30\n1/60\n1/125\n1/250\n1/500\n1/1000";
static const char* roller_aperture_options = "1.4\n2.0\n2.8\n4.0\n5.6\n8.0\n11.0\n16.0";
static const char* cam_name = "Minolta SR1-S";
static const char* len_name  = "Minolta MD 50mm f/1.4";
static const char* roller_iso_options = "50\n100\n125\n160\n200\n400\n800\n1600\n3200";
static const char* roller_ev_options = "-2\n-1\n-2/3\n-1/3\n0\n+1/3\n+2/3\n+1\n+2";

// 注意：原来是 const int*，但使用时当作值，改为普通 int
static int current_lux_value = 1000;

// ──────────────────────────────────────────────
// 全局 UI 对象指针（已在 .h 中 extern）
// ──────────────────────────────────────────────
lv_obj_t* main_table_time;              // 时间
lv_obj_t* main_table_status;            // 信号，电量
lv_obj_t* main_label_cam;               // 相机名字
lv_obj_t* main_label_len;               // 镜头名字
lv_obj_t* main_roller_shutter;          // 快门滚轮
lv_obj_t* main_roller_aperture;         // 光圈滚轮
lv_obj_t* main_roller_iso;              // iso值
lv_obj_t* main_roller_ev;               // ev值
lv_obj_t* main_label_lux_value;         // 光照强度
lv_obj_t* main_obj_mode_select;         // 模式选择容器


lv_obj_t* cam_win_select;               // cam选择
static lv_obj_t* cam_keyboard;        // 相机参数键盘
static lv_obj_t* num_keyboard;        // 数字键盘（用于焦距、闪光同步、自定义光圈）
// ──────────────────────────────────────────────
// 模式选择相关
// ──────────────────────────────────────────────
LV_IMG_DECLARE(img_cam);
LV_IMG_DECLARE(img_len);
LV_IMG_DECLARE(img_auto_mode);
LV_IMG_DECLARE(img_landscape_mode);
LV_IMG_DECLARE(img_manual_mode);
LV_IMG_DECLARE(img_portrait_mode);

static const lv_img_dsc_t * mode_icons[] = {
    &img_auto_mode,
    &img_landscape_mode,
    &img_manual_mode,
    &img_portrait_mode,
};

static uint8_t selected_idx = 0;  // 默认选中第0个 (从0开始，手动模式)

// ──────────────────────────────────────────────
// 相机参数设置相关
// ──────────────────────────────────────────────
static lv_obj_t *cam_card_win_container = NULL;
// static uint8_t cam_count = 0;
static lv_obj_t *cam_selected_card = NULL;      // 当前选中的卡片（NULL = 无选中）
static lv_obj_t *cam_pending_delete_card = NULL; // 待删除的卡片
static lv_coord_t cam_saved_scroll_y = 0;        // 保存的滚动位置
static lv_obj_t *cam_scroll_parent = NULL;       // 需要恢复滚动位置的父对象

// 函数声明
static void update_main_ui_from_cam_card(void);           // 从相机卡片数据更新主界面显示
static void cam_show_delete_confirm_dialog(void);         // 显示相机删除确认对话框
static void cam_restore_scroll_timer_cb(lv_timer_t * timer); // 定时器回调：恢复相机页面滚动位置
static CAM extract_cam_from_card(lv_obj_t* card);       // 从相机卡片提取数据

// ──────────────────────────────────────────────
// 镜头参数设置相关
// ──────────────────────────────────────────────
static lv_obj_t *len_card_win_container = NULL;
static lv_obj_t *len_selected_card = NULL;      // 当前选中的镜头卡片（NULL = 无选中）
static lv_obj_t *len_pending_delete_card = NULL; // 待删除的镜头卡片
static lv_obj_t *len_win_select = NULL;          // 镜头选择窗口
static lv_coord_t len_saved_scroll_y = 0;        // 保存的滚动位置
static lv_obj_t *len_scroll_parent = NULL;       // 需要恢复滚动位置的父对象

// 镜头卡片参数结构体
typedef struct {
    lv_obj_t *dropdown_aperture_step;  // 光圈挡位选择
    lv_obj_t *dropdown_min_aperture;   // 最小光圈选择
    lv_obj_t *dropdown_max_aperture;   // 最大光圈选择
    lv_obj_t *textarea_custom_aperture; // 自定义光圈输入框
    lv_obj_t *textarea_focal_length;   // 焦距输入框
    float *custom_aperture_array;      // 自定义光圈数组
    int custom_aperture_count;         // 自定义光圈数量
    int current_step_type;              // 0 = 1 档，1 = 1/2 档，2 = 1/3 档，3 = 自定义
} len_card_params_t;

// 函数声明
static void update_main_ui_from_len_card(void);           // 从镜头卡片数据更新主界面显示
static void len_delete_confirm_event_cb(lv_event_t * e);  // 镜头删除确认对话框按钮事件回调
static void len_show_delete_confirm_dialog(void);         // 显示镜头删除确认对话框
static LEN extract_len_from_card(lv_obj_t* card);       // 从镜头卡片提取数据
static void ui_setting_page_init(lv_obj_t* parent);

// ──────────────────────────────────────────────
// 辅助函数：快门显示格式化 (修复重复Bug)
// ──────────────────────────────────────────────
static void format_shutter_string(char* buf, size_t size, float shutter) {
    if (shutter <= 0) {
        snprintf(buf, size, "---");
    } else if (shutter >= 1.0f) {
        // >= 1秒，显示小数，例如 1.5s, 30s
        if (shutter == floorf(shutter)) {
            snprintf(buf, size, "%.0f\"", (double)shutter);
        } else {
            snprintf(buf, size, "%.1f\"", (double)shutter);
        }
    } else {
        // < 1秒，判断是否为标准整档分数
        float denom = 1.0f / shutter;
        float denom_rounded = roundf(denom);

        // 只有当倒数非常接近整数时（误差 < 0.05），才使用分数显示
        // 这样 0.5 -> 1/2, 0.25 -> 1/4
        // 但 0.8, 0.6, 0.3 等中间档位会保持小数显示，避免重复
        if (fabsf(denom - denom_rounded) < 0.05f) {
            snprintf(buf, size, "1/%d", (int)denom_rounded);
        } else {
            // 非标准分数档位，直接显示小数秒，例如 0.8", 0.3"
            snprintf(buf, size, "%.1f\"", (double)shutter);
        }
    }
}


// ──────────────────────────────────────────────
// 辅助函数：生成快门选项字符串
// ──────────────────────────────────────────────
static char* generate_shutter_options(const float* shutter_array, int count) {
    static char options[1024];
    int pos = 0;

    for (int i = 0; i < count && pos < sizeof(options) - 20; i++) {
        char buf[32];
        format_shutter_string(buf, sizeof(buf), shutter_array[i]);
        int len = strlen(buf);
        if (pos + len + 2 > sizeof(options)) break;
        if (i > 0) {
            options[pos++] = '\n';
        }
        strcpy(&options[pos], buf);
        pos += len;
    }
    options[pos] = '\0';
    return options;
}

// 辅助函数：按步长生成快门选项字符串（用于1档快门）
// ──────────────────────────────────────────────
char* app_ui_generate_shutter_options(const float* shutter_array, int count, int stride) {
    static char options[1024];
    int pos = 0;

    for (int i = 0; i < count && pos < sizeof(options) - 20; i += stride) {
        char buf[32];
        format_shutter_string(buf, sizeof(buf), shutter_array[i]);
        int len = strlen(buf);
        if (pos + len + 2 > sizeof(options)) break;
        if (i > 0) {
            options[pos++] = '\n';
        }
        strcpy(&options[pos], buf);
        pos += len;
    }
    options[pos] = '\0';
    return options;
}

// ──────────────────────────────────────────────
// 辅助函数：光圈显示格式化
// ──────────────────────────────────────────────
static void format_aperture_string(char* buf, size_t size, float aperture) {
    if (aperture <= 0) {
        snprintf(buf, size, "---");
    } else {
        snprintf(buf, size, "%.1f", (double)aperture);
    }
}

// ──────────────────────────────────────────────
// 辅助函数：生成光圈选项字符串
// ──────────────────────────────────────────────
static char* generate_aperture_options(const float* aperture_array, int count) {
    static char options[1024];
    int pos = 0;

    for (int i = 0; i < count && pos < sizeof(options) - 20; i++) {
        char buf[32];
        format_aperture_string(buf, sizeof(buf), aperture_array[i]);
        int len = strlen(buf);
        if (pos + len + 2 > sizeof(options)) break;
        if (i > 0) {
            options[pos++] = '\n';
        }
        strcpy(&options[pos], buf);
        pos += len;
    }
    options[pos] = '\0';
    return options;
}

// 辅助函数：按步长生成光圈选项字符串（用于1档光圈）
// ──────────────────────────────────────────────
char* app_ui_generate_aperture_options(const float* aperture_array, int count, int stride) {
    static char options[1024];
    int pos = 0;

    for (int i = 0; i < count && pos < sizeof(options) - 20; i += stride) {
        char buf[32];
        format_aperture_string(buf, sizeof(buf), aperture_array[i]);
        int len = strlen(buf);
        if (pos + len + 2 > sizeof(options)) break;
        if (i > 0) {
            options[pos++] = '\n';
        }
        strcpy(&options[pos], buf);
        pos += len;
    }
    options[pos] = '\0';
    return options;
}

// ──────────────────────────────────────────────
// 辅助函数：解析自定义光圈字符串
// 输入格式："1.4,2,2.8,4" 或 "1.4, 2, 2.8, 4"
// 返回：动态分配的光圈数组，调用者需要负责释放
// ──────────────────────────────────────────────
static float* parse_custom_aperture_string(const char* str, int* out_count) {
    if (!str || !out_count) {
        if (out_count) *out_count = 0;
        return NULL;
    }

    // 临时数组，用于存储解析的光圈值
    float temp_array[100];
    int count = 0;

    char buffer[256];
    strncpy(buffer, str, sizeof(buffer) - 1);
    buffer[sizeof(buffer) - 1] = '\0';

    char* token = strtok(buffer, ",");
    while (token != NULL && count < 100) {
        // 去除前后空格
        while (*token == ' ') token++;
        char* end = token + strlen(token) - 1;
        while (end > token && *end == ' ') end--;
        *(end + 1) = '\0';

        float value = atof(token);
        if (value > 0) {
            temp_array[count++] = value;
        }
        token = strtok(NULL, ",");
    }

    if (count == 0) {
        *out_count = 0;
        return NULL;
    }

    // 分配内存并复制数据
    float* result = (float*)malloc(count * sizeof(float));
    if (result) {
        memcpy(result, temp_array, count * sizeof(float));
        *out_count = count;
    } else {
        *out_count = 0;
    }

    return result;
}


// ──────────────────────────────────────────────
// 滚轮样式清理函数
// ──────────────────────────────────────────────
void style_roller_clean_style(lv_obj_t* roller, bool select_line)
{
    lv_obj_set_style_text_font(roller, &lv_font_montserrat_20, LV_STATE_DEFAULT);

    /* 1. 整体透明 + 去掉所有边框阴影 */
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(roller, 0, 0);
    lv_obj_set_style_outline_width(roller, 0, 0);
    lv_obj_set_style_shadow_width(roller, 0, 0);

    if (select_line)
    {
        // 添加下划线（最专业的选中指示方式）
        lv_obj_set_style_border_side(roller, LV_BORDER_SIDE_BOTTOM, LV_PART_SELECTED);
        lv_obj_set_style_border_width(roller, 2, LV_PART_SELECTED);
        lv_obj_set_style_border_color(roller, lv_color_white(), LV_PART_SELECTED);
        lv_obj_set_style_border_opa(roller, LV_OPA_30, LV_PART_SELECTED);   // 淡淡的线，高级感拉满
    }

    /* 4. 最关键！强制清除默认的蓝色背景条 */
    lv_obj_set_style_bg_opa(roller, LV_OPA_TRANSP, LV_PART_SELECTED);
    lv_obj_set_style_bg_color(roller, lv_color_hex(0x000000), LV_PART_SELECTED); // 颜色随意，只要透明就行

    /* 5. 显示 5 行 */
    lv_roller_set_visible_row_count(roller, 4);
}

// ──────────────────────────────────────────────
// 图片根据父对象自适应大小
// ──────────────────────────────────────────────
void style_img_set_size_accordance_obj(lv_obj_t* parent, lv_obj_t* img_obj, const lv_img_dsc_t img)
{
    lv_img_header_t header;
    if (lv_img_decoder_get_info(&img, &header) != LV_RES_OK) {
        // 可以加 log 或默认值
        lv_img_set_zoom(img_obj, LV_IMG_ZOOM_NONE);
        return;
    }

    lv_coord_t parent_w = lv_obj_get_width(parent);
    lv_coord_t parent_h = lv_obj_get_height(parent);
    if (parent_w <= 0 || parent_h <= 0) {
        lv_img_set_zoom(img_obj, LV_IMG_ZOOM_NONE);
        return;
    }

    lv_coord_t img_w = header.w;
    lv_coord_t img_h = header.h;
    if (img_w <= 0 || img_h <= 0) {
        lv_img_set_zoom(img_obj, LV_IMG_ZOOM_NONE);
        return;
    }

    uint16_t scale = LV_MIN(
        (uint32_t)parent_w * 256 / img_w,
        (uint32_t)parent_h * 256 / img_h
    );

    if (scale < 64)  scale = 64;
    if (scale > 512) scale = 512;

    lv_img_set_zoom(img_obj, scale);
    lv_img_set_antialias(img_obj, true);      // 缩放更平滑
    lv_obj_center(img_obj);                   // 居中显示
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
}

// ──────────────────────────────────────────────
// 模式选择按钮事件回调
// ──────────────────────────────────────────────
static void imgbtn_event_cb(lv_event_t * e) {
    lv_obj_t * imgbtn = lv_event_get_target(e);
    uint32_t idx = (uint32_t)(uintptr_t)lv_event_get_user_data(e);

    if (selected_idx == idx) return;

    // 取消上一个选中 → 恢复透明背景 + 细暗边
    lv_obj_t * prev = lv_obj_get_child(lv_obj_get_parent(imgbtn), selected_idx);
    lv_obj_set_style_bg_opa(prev, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(prev, 1, 0);
    lv_obj_set_style_border_color(prev, lv_color_hex(0x404040), 0);
    lv_obj_set_style_border_opa(prev, LV_OPA_60, 0);

    // 选中当前 → 蓝色背景 + 亮蓝边
    lv_obj_set_style_bg_color(imgbtn, lv_color_hex(0x4C5C6E), 0);
    lv_obj_set_style_bg_opa(imgbtn, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(imgbtn, 2, 0);
    lv_obj_set_style_border_color(imgbtn, lv_color_hex(0x6A8ABF), 0);

    selected_idx = idx;
    LV_LOG_USER("Selected mode: %d", idx);
}

// ──────────────────────────────────────────────
// Tv/Av滚轮事件回调 - 滑动时自动切换到手动模式
// ──────────────────────────────────────────────
#define UI_IDX_MANUAL_MODE 2  // mode_icons数组中手动模式的索引

static void roller_manual_mode_event_cb(lv_event_t * e) {
    lv_obj_t * roller = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    LV_LOG_USER("Roller changed: selected_idx=%d, roller=%p, shutter=%p, aperture=%p",
                selected_idx, (void*)roller, (void*)main_roller_shutter, (void*)main_roller_aperture);

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (selected_idx != UI_IDX_MANUAL_MODE) {
            lv_obj_t * btn = lv_obj_get_child(main_obj_mode_select, UI_IDX_MANUAL_MODE);

            if (btn) {
                if (selected_idx >= 0 && selected_idx < 4) {
                    lv_obj_t * prev = lv_obj_get_child(main_obj_mode_select, selected_idx);
                    if (prev) {
                        lv_obj_set_style_bg_opa(prev, LV_OPA_TRANSP, 0);
                        lv_obj_set_style_border_width(prev, 1, 0);
                        lv_obj_set_style_border_color(prev, lv_color_hex(0x404040), 0);
                        lv_obj_set_style_border_opa(prev, LV_OPA_60, 0);
                    }
                }

                lv_obj_set_style_bg_color(btn, lv_color_hex(0x4C5C6E), 0);
                lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
                lv_obj_set_style_border_width(btn, 2, 0);
                lv_obj_set_style_border_color(btn, lv_color_hex(0x6A8ABF), 0);

                selected_idx = UI_IDX_MANUAL_MODE;
                LV_LOG_USER("Auto switch to manual mode (UI idx %d) due to roller change", UI_IDX_MANUAL_MODE);
            }
        }

        if (roller == main_roller_shutter) {
            ui_calc_port_set_manual_wheel_type(MANUAL_WHEEL_TV);
            LV_LOG_USER("Manual wheel: TV");
        } else if (roller == main_roller_aperture) {
            ui_calc_port_set_manual_wheel_type(MANUAL_WHEEL_AV);
            LV_LOG_USER("Manual wheel: AV");
        }
    }
}

// ──────────────────────────────────────────────
// 创建模式选择组件
// ──────────────────────────────────────────────
static void create_mode_selector(lv_obj_t * parent) {
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(parent, 5, 0);
    lv_obj_set_style_pad_all(parent, 4, 0);

    // 关键：彻底禁止整个容器滚动和拖动
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_SCROLL_CHAIN);  // 额外清除链式滚动

    lv_coord_t parent_w = lv_obj_get_width(parent);
    lv_coord_t parent_h = lv_obj_get_height(parent);

    lv_coord_t ref_size = LV_MIN(
        (lv_coord_t)(parent_w * 0.88f / 5.4f),
        parent_h - 18
    );

    if (ref_size < 28) ref_size = 28;
    if (ref_size > 68) ref_size = 68;

    lv_coord_t btn_size = ref_size;

    uint16_t zoom_val = (uint16_t)((uint32_t)btn_size * 256 / 128);
    if (zoom_val < 56) zoom_val = 56;
    if (zoom_val > 256) zoom_val = 256;

    lv_coord_t offset_adj = (btn_size <= 44) ? 0 : -1;

    for (uint32_t i = 0; i < 4; i++) {
        lv_obj_t * btn = lv_imgbtn_create(parent);

        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_RELEASED, NULL, NULL, NULL);
        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_PRESSED, NULL, NULL, NULL);
        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_CHECKED_RELEASED, NULL, NULL, NULL);
        lv_imgbtn_set_src(btn, LV_IMGBTN_STATE_CHECKED_PRESSED, NULL, NULL, NULL);

        lv_obj_set_size(btn, btn_size, btn_size);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);

        // 关键：禁止按钮本身滚动和拖动
        lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(btn, LV_SCROLLBAR_MODE_OFF);

        lv_obj_t * icon = lv_img_create(btn);
        lv_img_set_src(icon, mode_icons[i]);
        lv_obj_set_size(icon, btn_size, btn_size);
        lv_img_set_size_mode(icon, LV_IMG_SIZE_MODE_REAL);
        lv_img_set_zoom(icon, zoom_val);
        lv_img_set_antialias(icon, true);

        lv_obj_set_style_img_recolor(icon, lv_color_white(), 0);
        lv_obj_set_style_img_recolor_opa(icon, LV_OPA_COVER, 0);

        lv_obj_center(icon);
        lv_img_set_offset_x(icon, offset_adj - 2);
        lv_img_set_offset_y(icon, offset_adj - 2);

        // 关键：禁止图片控件被拖动（最重要的一行）
        lv_obj_clear_flag(icon, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_PRESS_LOCK);
        lv_obj_add_flag(icon, LV_OBJ_FLAG_EVENT_BUBBLE);  // 让点击事件冒泡到父按钮

        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x2a2a3e), 0);
        lv_obj_set_style_radius(btn, LV_MIN(9, btn_size / 7), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_opa(btn, LV_OPA_50, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        if (i == selected_idx) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x4C5C6E), 0);     // 调亮后的青灰
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);                 // 强制完全不透明
            lv_obj_set_style_border_width(btn, 2, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x6A8ABF), 0); // 更亮的边框
        }

        lv_obj_add_event_cb(btn, imgbtn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
}

// ──────────────────────────────────────────────
// 长按相机容器的事件回调
// ──────────────────────────────────────────────
// ──────────────────────────────────────────────
// 长按相机区域 → 只弹出一个空的窗口（供你后续自定义内容）
// ──────────────────────────────────────────────
// ──────────────────────────────────────────────
// 长按相机区域 → 弹出带标题的空窗口（LVGL 8.4.0 兼容）
// ──────────────────────────────────────────────
// 相机关闭窗口按钮事件回调
// ──────────────────────────────────────────────
static void cam_close_win_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);                    /* 获取事件类型 */
    if (code == LV_EVENT_CLICKED)
    {
        // 先收起键盘
        lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(num_keyboard, LV_OBJ_FLAG_HIDDEN);
        // 再关闭窗口
        lv_obj_add_flag(cam_win_select, LV_OBJ_FLAG_HIDDEN);
    }

}

// ──────────────────────────────────────────────
// 相机卡片长按事件回调（显示删除确认对话框）
// ──────────────────────────────────────────────
static void cam_long_press_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_LONG_PRESSED)
    {
        lv_obj_clear_flag(cam_win_select, LV_OBJ_FLAG_HIDDEN);
    }
}

// ──────────────────────────────────────────────
// 相机参数设置相关回调函数
// ──────────────────────────────────────────────

/**
 * @brief 定时器回调函数，用于延迟恢复相机页面滚动位置
 * 在 LVGL 完成所有布局更新和显示刷新后执行
 */
static void cam_restore_scroll_timer_cb(lv_timer_t * timer)
{
    lv_obj_t *scroll_parent = cam_scroll_parent;
    
    if (cam_scroll_parent) {
        lv_obj_scroll_to_y(cam_scroll_parent, cam_saved_scroll_y, LV_ANIM_OFF);
        cam_scroll_parent = NULL;
    }
    // 确保键盘保持隐藏
    extern lv_obj_t* cam_keyboard;
    if (cam_keyboard) {
        lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 清除所有输入框的焦点状态
    if (scroll_parent) {
        uint32_t parent_child_cnt = lv_obj_get_child_cnt(scroll_parent);
        for (uint32_t i = 0; i < parent_child_cnt; i++) {
            lv_obj_t *card = lv_obj_get_child(scroll_parent, i);
            if (card) {
                lv_obj_t *content = lv_obj_get_child(card, 0);
                if (content) {
                    uint32_t content_child_cnt = lv_obj_get_child_cnt(content);
                    for (uint32_t j = 0; j < content_child_cnt; j++) {
                        lv_obj_t *child = lv_obj_get_child(content, j);
                        if (lv_obj_check_type(child, &lv_textarea_class)) {
                            lv_obj_clear_state(child, LV_STATE_FOCUSED);
                            lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                            // 移除键盘标志，防止显示光标
                            lv_textarea_set_cursor_click_pos(child, false);
                        }
                    }
                }
            }
        }
    }
    
    // 更新主界面
    update_main_ui_from_cam_card();
    
    lv_timer_del(timer);
}

/**
 * @brief 定时器回调函数，用于延迟恢复镜头页面滚动位置
 * 在 LVGL 完成所有布局更新和显示刷新后执行
 */
static void len_restore_scroll_timer_cb(lv_timer_t * timer)
{
    lv_obj_t *scroll_parent = len_scroll_parent;
    
    if (len_scroll_parent) {
        lv_obj_scroll_to_y(len_scroll_parent, len_saved_scroll_y, LV_ANIM_OFF);
        len_scroll_parent = NULL;
    }
    // 确保键盘保持隐藏
    extern lv_obj_t* cam_keyboard;
    if (cam_keyboard) {
        lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
    }
    
    // 清除所有输入框的焦点状态
    if (scroll_parent) {
        uint32_t parent_child_cnt = lv_obj_get_child_cnt(scroll_parent);
        for (uint32_t i = 0; i < parent_child_cnt; i++) {
            lv_obj_t *card = lv_obj_get_child(scroll_parent, i);
            if (card) {
                lv_obj_t *content = lv_obj_get_child(card, 0);
                if (content) {
                    uint32_t content_child_cnt = lv_obj_get_child_cnt(content);
                    for (uint32_t j = 0; j < content_child_cnt; j++) {
                        lv_obj_t *child = lv_obj_get_child(content, j);
                        if (lv_obj_check_type(child, &lv_textarea_class)) {
                            lv_obj_clear_state(child, LV_STATE_FOCUSED);
                            lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                            // 移除键盘标志，防止显示光标
                            lv_textarea_set_cursor_click_pos(child, false);
                        }
                    }
                }
            }
        }
    }
    
    // 更新主界面
    update_main_ui_from_len_card();
    
    lv_timer_del(timer);
}

static void cam_delete_confirm_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *mbox = lv_event_get_user_data(e);

    if (code == LV_EVENT_CLICKED) {
        const char *txt = lv_label_get_text(lv_obj_get_child(btn, 0));

        if (strcmp(txt, "Yes") == 0) {
            // 用户点击了确认删除
            if (cam_pending_delete_card) {
                bool was_selected = (cam_selected_card == cam_pending_delete_card);

                lv_obj_t *parent = lv_obj_get_parent(cam_pending_delete_card);
                if (!parent) {
                    lv_obj_del_async(mbox);
                    cam_pending_delete_card = NULL;
                    return;
                }

                uint32_t child_cnt = lv_obj_get_child_cnt(parent);
                int32_t idx = -1;

                for (uint32_t i = 0; i < child_cnt; i++) {
                    if (lv_obj_get_child(parent, i) == cam_pending_delete_card) {
                        idx = (int32_t)i;
                        break;
                    }
                }

                if (idx >= 0) {
                    lv_obj_t *next_card = NULL;

                    if (idx + 1 < (int32_t)child_cnt) {
                        next_card = lv_obj_get_child(parent, idx + 1);
                    }
                    else if (idx > 0) {
                        next_card = lv_obj_get_child(parent, idx - 1);
                    }

                    /* 保存当前滚动位置和父对象指针 */
                    cam_saved_scroll_y = lv_obj_get_scroll_y(parent);
                    cam_scroll_parent = parent;

                    // 隐藏键盘并清除焦点
                    lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
                    lv_keyboard_set_textarea(cam_keyboard, NULL);

                    // 清除被删除卡片上所有输入框的焦点
                    lv_obj_t *card_content = lv_obj_get_child(cam_pending_delete_card, 0);
                    if (card_content) {
                        uint32_t content_child_cnt = lv_obj_get_child_cnt(card_content);
                        for (uint32_t i = 0; i < content_child_cnt; i++) {
                            lv_obj_t *child = lv_obj_get_child(card_content, i);
                            if (lv_obj_check_type(child, &lv_textarea_class)) {
                                lv_obj_clear_state(child, LV_STATE_FOCUSED);
                                lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                            }
                        }
                    }

                    // 使用异步删除，避免在渲染过程中删除对象
                    lv_obj_del_async(cam_pending_delete_card);

                    // 删除后立即清除所有剩余卡片上输入框的焦点
                    uint32_t parent_child_cnt = lv_obj_get_child_cnt(parent);
                    for (uint32_t i = 0; i < parent_child_cnt; i++) {
                        lv_obj_t *card = lv_obj_get_child(parent, i);
                        if (card) {
                            lv_obj_t *content = lv_obj_get_child(card, 0);
                            if (content) {
                                uint32_t content_child_cnt = lv_obj_get_child_cnt(content);
                                for (uint32_t j = 0; j < content_child_cnt; j++) {
                                    lv_obj_t *child = lv_obj_get_child(content, j);
                                    if (lv_obj_check_type(child, &lv_textarea_class)) {
                                        lv_obj_clear_state(child, LV_STATE_FOCUSED);
                                        lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                                    }
                                }
                            }
                        }
                    }
                    
                    // 删除后再次清除所有输入框的焦点（防止 LVGL 自动聚焦）
                    for (uint32_t i = 0; i < parent_child_cnt; i++) {
                        lv_obj_t *card = lv_obj_get_child(parent, i);
                        if (card) {
                            lv_obj_t *content = lv_obj_get_child(card, 0);
                            if (content) {
                                uint32_t content_child_cnt = lv_obj_get_child_cnt(content);
                                for (uint32_t j = 0; j < content_child_cnt; j++) {
                                    lv_obj_t *child = lv_obj_get_child(content, j);
                                    if (lv_obj_check_type(child, &lv_textarea_class)) {
                                        lv_obj_clear_state(child, LV_STATE_FOCUSED);
                                        lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                                    }
                                }
                            }
                        }
                    }

                    if (was_selected) {
                        cam_selected_card = next_card;

                        if (cam_selected_card) {
                            lv_obj_set_style_bg_color(cam_selected_card, lv_color_hex(0x2a2a3e), 0);
                            lv_obj_set_style_border_color(cam_selected_card, lv_color_hex(0x4a90e2), 0);
                            lv_obj_set_style_border_width(cam_selected_card, 2, 0);
                        }
                    }

                    if (lv_obj_get_child_cnt(parent) == 0) {
                        cam_selected_card = NULL;
                    }
                    
                    /* 创建定时器，在下一帧恢复滚动位置和更新主界面 */
                    lv_timer_t * timer = lv_timer_create(cam_restore_scroll_timer_cb, 1, NULL);
                    lv_timer_set_repeat_count(timer, 1);
                }
            }
        }

        // 关闭对话框
        lv_obj_del_async(mbox);
        cam_pending_delete_card = NULL;
    }
}

static void cam_show_delete_confirm_dialog(void)
{
    lv_obj_t *mbox = lv_obj_create(lv_layer_top());
    lv_obj_set_size(mbox, 300, 150);
    lv_obj_center(mbox);
    lv_obj_set_style_radius(mbox, 10, 0);
    lv_obj_set_style_bg_color(mbox, lv_color_black(), 0);
    // lv_obj_set_style_border_color(mbox, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_pad_all(mbox, 20, 0);
    lv_obj_set_flex_flow(mbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mbox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(mbox, LV_OPA_70, 0);

    lv_obj_t *title = lv_label_create(mbox);
    lv_label_set_text(title, "Confirm Delete");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *msg = lv_label_create(mbox);
    lv_label_set_text(msg, "Are you sure to delete this camera?");
    lv_obj_set_style_text_color(msg, lv_color_hex(0xcccccc), 0);

    lv_obj_t *btn_cont = lv_obj_create(mbox);
    lv_obj_set_size(btn_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);

    lv_obj_t *btn_cancel = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_cancel, 100, 36);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x666666), 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x555555), LV_STATE_PRESSED);

    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_center(lbl_cancel);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);

    lv_obj_t *btn_confirm = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_confirm, 100, 36);
    lv_obj_set_style_radius(btn_confirm, 6, 0);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xe74c3c), 0);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xc0392b), LV_STATE_PRESSED);

    lv_obj_t *lbl_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(lbl_confirm, "Yes");
    lv_obj_center(lbl_confirm);
    lv_obj_set_style_text_color(lbl_confirm, lv_color_white(), 0);

    lv_obj_add_event_cb(btn_cancel, cam_delete_confirm_event_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_add_event_cb(btn_confirm, cam_delete_confirm_event_cb, LV_EVENT_CLICKED, mbox);
}

// ──────────────────────────────────────────────
// 相机卡片点击事件回调（选中/取消选中）
// ──────────────────────────────────────────────
static void cam_card_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *card = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        // 单击 → 选中（逻辑不变）
        if (cam_selected_card != card) {
            if (cam_selected_card) {
                lv_obj_set_style_bg_color(cam_selected_card, lv_color_hex(0x1e1e2e), 0);
                lv_obj_set_style_border_color(cam_selected_card, lv_color_hex(0x444466), 0);
                lv_obj_set_style_border_width(cam_selected_card, 1, 0);
            }

            lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a3e), 0);
            lv_obj_set_style_border_color(card, lv_color_hex(0x4a90e2), 0);
            lv_obj_set_style_border_width(card, 2, 0);

            cam_selected_card = card;

            // 更新主界面
            update_main_ui_from_cam_card();
        }
    }
    else if (code == LV_EVENT_LONG_PRESSED) {
        // 长按 → 弹出确认对话框
        cam_pending_delete_card = card;
        cam_show_delete_confirm_dialog();
    }
}
// ──────────────────────────────────────────────
// 相机名字输入框事件回调
// ──────────────────────────────────────────────
static void cam_ta_name_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if(code == LV_EVENT_CLICKED) {
        /*只在点击时弹出键盘，避免自动聚焦时弹出*/
        if(cam_keyboard != NULL) {
            lv_keyboard_set_textarea(cam_keyboard, ta);
            lv_obj_clear_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(cam_keyboard);
            // 防止输入框自动获得焦点
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
}

// ──────────────────────────────────────────────
// 键盘完成/取消事件回调（通用）
// ──────────────────────────────────────────────
static void keyboard_done_cancel_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * keyboard = lv_event_get_target(e);
    
    if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(keyboard, LV_OBJ_FLAG_HIDDEN);
        // 更新主界面
        update_main_ui_from_cam_card();
        update_main_ui_from_len_card();
    }
}

// ──────────────────────────────────────────────
// 相机卡片参数数据结构
// ──────────────────────────────────────────────
typedef struct {
    lv_obj_t *dropdown_shutter_step;  // 快门挡位选择
    lv_obj_t *dropdown_min_shutter;   // 最小快门选择
    lv_obj_t *dropdown_max_shutter;   // 最大快门选择
    lv_obj_t *textarea_flash_sync;    // 闪光同步值输入框
    int current_step_type;            // 0 = 1 档，1 = 1/2 档，2 = 1/3 档
} cam_card_params_t;

// ──────────────────────────────────────────────
// 快门挡位选择事件回调
// ──────────────────────────────────────────────
static void shutter_step_event_cb(lv_event_t * e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    cam_card_params_t *params = (cam_card_params_t *)lv_event_get_user_data(e);

    uint32_t selected = lv_dropdown_get_selected(dropdown);
    params->current_step_type = (int)selected;

    // 根据挡位类型更新快门选项
    const float *shutter_array;
    int count;
    int stride = 1;

    if (selected == 0) {
        // 1档 - 使用SHUTTERS_1_3表，步长为3
        shutter_array = SHUTTERS_1_3;
        count = COUNT_SHUTTERS_1_3;
        stride = 3;
    } else if (selected == 1) {
        // 1/2档
        shutter_array = SHUTTERS_1_2;
        count = COUNT_SHUTTERS_1_2;
        stride = 1;
    } else {
        // 1/3档
        shutter_array = SHUTTERS_1_3;
        count = COUNT_SHUTTERS_1_3;
        stride = 1;
    }

    char *options = app_ui_generate_shutter_options(shutter_array, count, stride);

    // 更新最小快门和最大快门的选项
    lv_dropdown_set_options(params->dropdown_min_shutter, options);
    lv_dropdown_set_options(params->dropdown_max_shutter, options);

    // 重置选择到中间位置
    int actual_count = (count + stride - 1) / stride;
    lv_dropdown_set_selected(params->dropdown_min_shutter, actual_count - 1);
    lv_dropdown_set_selected(params->dropdown_max_shutter, 0);

    // 更新主界面
    update_main_ui_from_cam_card();
}

// ──────────────────────────────────────────────
// 最小快门选择事件回调
// ──────────────────────────────────────────────
static void min_shutter_event_cb(lv_event_t * e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    cam_card_params_t *params = (cam_card_params_t *)lv_event_get_user_data(e);

    uint32_t min_idx = lv_dropdown_get_selected(dropdown);
    uint32_t max_idx = lv_dropdown_get_selected(params->dropdown_max_shutter);

    // 验证：最小快门不能超过最大快门
    if (min_idx < max_idx) {
        // 如果最小快门小于最大快门，需要调整
        lv_dropdown_set_selected(dropdown, max_idx);
    }

    // 更新主界面
    update_main_ui_from_cam_card();
}

// ──────────────────────────────────────────────
// 最大快门选择事件回调
// ──────────────────────────────────────────────
static void max_shutter_event_cb(lv_event_t * e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    cam_card_params_t *params = (cam_card_params_t *)lv_event_get_user_data(e);

    uint32_t max_idx = lv_dropdown_get_selected(dropdown);
    uint32_t min_idx = lv_dropdown_get_selected(params->dropdown_min_shutter);

    // 验证：最大快门不能小于最小快门
    if (max_idx > min_idx) {
        // 如果最大快门大于最小快门，需要调整
        lv_dropdown_set_selected(dropdown, min_idx);
    }

    // 更新主界面
    update_main_ui_from_cam_card();
}

// ──────────────────────────────────────────────
// 从选中的相机卡片更新主界面
// ──────────────────────────────────────────────
static void update_main_ui_from_cam_card(void)
{
    if (!cam_selected_card) {
        // 没有选中的卡片，显示默认值
        if (main_label_cam) {
            lv_label_set_text(main_label_cam, "----");
        }
        if (main_roller_shutter) {
            // 显示默认的1档快门（使用步长3）
            char *options = app_ui_generate_shutter_options(SHUTTERS_1_3, COUNT_SHUTTERS_1_3, 3);
            lv_roller_set_options(main_roller_shutter, options, LV_ROLLER_MODE_NORMAL);
        }
        return;
    }

    // 获取卡片的内容容器
    lv_obj_t *card_content = lv_obj_get_child(cam_selected_card, 0);
    if (!card_content) {
        return;
    }

    // 获取第一行容器（第1个子对象）
    lv_obj_t *row1 = lv_obj_get_child(card_content, 0);
    // 获取第二行容器（第2个子对象）
    lv_obj_t *row2 = lv_obj_get_child(card_content, 1);

    if (!row1 || !row2) {
        return;
    }

    // 获取相机名称输入框（row1的第1个子对象）
    lv_obj_t *cam_ta_name = lv_obj_get_child(row1, 0);
    if (cam_ta_name) {
        const char *name = lv_textarea_get_text(cam_ta_name);
        if (name && strlen(name) > 0) {
            lv_label_set_text(main_label_cam, name);
        } else {
            lv_label_set_text(main_label_cam, "----");
        }
    }

    // 获取快门挡位下拉框（row1的第2个子对象）
    lv_obj_t *dropdown_shutter_step = lv_obj_get_child(row1, 1);
    // 获取最小快门下拉框（row2的第1个子对象）
    lv_obj_t *dropdown_min_shutter = lv_obj_get_child(row2, 0);
    // 获取最大快门下拉框（row2的第2个子对象）
    lv_obj_t *dropdown_max_shutter = lv_obj_get_child(row2, 1);

    if (dropdown_shutter_step && dropdown_min_shutter && dropdown_max_shutter) {
        // 获取当前选中的挡位类型
        uint32_t step_type = lv_dropdown_get_selected(dropdown_shutter_step);

        // 获取最小和最大快门的索引（dropdown中的索引）
        uint32_t min_idx = lv_dropdown_get_selected(dropdown_min_shutter);
        uint32_t max_idx = lv_dropdown_get_selected(dropdown_max_shutter);

        // 根据挡位类型选择源表和步长
        const float *shutter_array;
        int count;
        int stride = 1;

        if (step_type == 0) {
            // 1档
            shutter_array = SHUTTERS_1_3;
            count = COUNT_SHUTTERS_1_3;
            stride = 3;
        } else if (step_type == 1) {
            // 1/2档
            shutter_array = SHUTTERS_1_2;
            count = COUNT_SHUTTERS_1_2;
            stride = 1;
        } else {
            // 1/3档
            shutter_array = SHUTTERS_1_3;
            count = COUNT_SHUTTERS_1_3;
            stride = 1;
        }

        // 将dropdown索引转换为原始数组索引
        // dropdown中的索引需要乘以步长才能得到原始数组的索引
        int orig_min_idx = (int)min_idx * stride;
        int orig_max_idx = (int)max_idx * stride;

        // 生成快门选项字符串（从orig_min_idx到orig_max_idx，按步长stride）
        static char options[1024];
        int pos = 0;

        for (int i = orig_min_idx; i >= orig_max_idx && i >= 0 && i < count && pos < sizeof(options) - 20; i -= stride) {
            char buf[32];
            format_shutter_string(buf, sizeof(buf), shutter_array[i]);
            int len = strlen(buf);
            if (pos + len + 2 > sizeof(options)) break;
            if (i != orig_min_idx) {
                options[pos++] = '\n';
            }
            strcpy(&options[pos], buf);
            pos += len;
        }
        options[pos] = '\0';

        // 更新主界面的快门滚轮
        lv_roller_set_options(main_roller_shutter, options, LV_ROLLER_MODE_NORMAL);
    }
}

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

static CAM extract_cam_from_card(lv_obj_t* card)
{
    CAM cam = {0};
    cam.shutter_stops = NULL;
    cam.shutter_stop_count = 0;
    cam.flash_sync_shutter = 0.0f;  // 0 表示无闪光同步限制

    if (!card) {
        return cam;
    }

    lv_obj_t *card_content = lv_obj_get_child(card, 0);
    if (!card_content) {
        return cam;
    }

    // 获取第一行容器（第1个子对象）
    lv_obj_t *row1 = lv_obj_get_child(card_content, 0);
    // 获取第二行容器（第2个子对象）
    lv_obj_t *row2 = lv_obj_get_child(card_content, 1);

    if (!row1 || !row2) {
        return cam;
    }

    lv_obj_t *dropdown_shutter_step = lv_obj_get_child(row1, 1);
    lv_obj_t *dropdown_min_shutter = lv_obj_get_child(row2, 0);
    lv_obj_t *dropdown_max_shutter = lv_obj_get_child(row2, 1);
    lv_obj_t *textarea_flash_sync = lv_obj_get_child(row2, 2);

    if (dropdown_shutter_step && dropdown_min_shutter && dropdown_max_shutter) {
        uint32_t step_type = lv_dropdown_get_selected(dropdown_shutter_step);
        uint32_t min_idx = lv_dropdown_get_selected(dropdown_min_shutter);
        uint32_t max_idx = lv_dropdown_get_selected(dropdown_max_shutter);

        const float *shutter_array;
        int count;
        int stride = 1;

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

        int orig_min_idx = (int)min_idx * stride;
        int orig_max_idx = (int)max_idx * stride;

        int actual_count = 0;
        for (int i = orig_min_idx; i >= orig_max_idx && i >= 0 && i < count; i -= stride) {
            actual_count++;
        }

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

    if (textarea_flash_sync) {
        const char* flash_sync_str = lv_textarea_get_text(textarea_flash_sync);
        if (flash_sync_str && strlen(flash_sync_str) > 0) {
            cam.flash_sync_shutter = parse_flash_sync_value(flash_sync_str);
        }
    }

    return cam;
}

// ──────────────────────────────────────────────
// 数字键盘输入事件回调（用于焦距、闪光同步）
// ──────────────────────────────────────────────
static void num_ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if(code == LV_EVENT_CLICKED) {
        // 只在点击时弹出数字键盘
        if(num_keyboard != NULL) {
            lv_keyboard_set_textarea(num_keyboard, ta);
            lv_obj_clear_flag(num_keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(num_keyboard);
            // 防止输入框自动获得焦点
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
}

// ──────────────────────────────────────────────
// 自定义光圈输入框事件回调（使用带逗号的自定义键盘）
// ──────────────────────────────────────────────
static void custom_aperture_ta_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if(code == LV_EVENT_CLICKED) {
        // 只在点击时弹出数字键盘
        if(num_keyboard != NULL) {
            // 使用自定义键盘映射（带逗号）
            lv_keyboard_set_map(num_keyboard, LV_KEYBOARD_MODE_NUMBER, custom_aperture_kb_map, custom_aperture_kb_ctrl);
            lv_keyboard_set_textarea(num_keyboard, ta);
            lv_obj_clear_flag(num_keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(num_keyboard);
            // 防止输入框自动获得焦点
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
}

// ──────────────────────────────────────────────
// 镜头名字输入事件回调
// ──────────────────────────────────────────────
static void len_ta_name_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if(code == LV_EVENT_CLICKED) {
        // 检查是否是自定义光圈模式
        lv_obj_t *card = len_selected_card;
        if (card) {
            lv_obj_t *card_content = lv_obj_get_child(card, 0);
            if (card_content) {
                lv_obj_t *row1 = lv_obj_get_child(card_content, 0);
                if (row1) {
                    lv_obj_t *dropdown_aperture_step = lv_obj_get_child(row1, 1);
                    if (dropdown_aperture_step) {
                        uint32_t step_type = lv_dropdown_get_selected(dropdown_aperture_step);
                        // 如果是自定义光圈模式，不弹出键盘
                        if (step_type == 3) {
                            return;
                        }
                    }
                }
            }
        }

        // 只在点击时弹出键盘，避免自动聚焦时弹出
        if(cam_keyboard != NULL) {
            lv_keyboard_set_textarea(cam_keyboard, ta);
            lv_obj_clear_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
            lv_obj_move_foreground(cam_keyboard);
            // 防止输入框自动获得焦点
            lv_indev_wait_release(lv_indev_get_act());
        }
    }
    else if(code == LV_EVENT_FOCUSED) {
        // 如果是自动聚焦（非用户点击），立即清除焦点
        if(cam_keyboard && !lv_obj_has_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN)) {
            // 键盘已经显示，说明是用户操作，保持焦点
        } else {
            // 键盘隐藏，说明是自动聚焦，清除焦点
            lv_obj_clear_state(ta, LV_STATE_FOCUSED);
            lv_obj_clear_state(ta, LV_STATE_FOCUS_KEY);
        }
    }
    else if(code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
        if(code == LV_EVENT_READY) {
            // 用户完成输入，更新主界面
            update_main_ui_from_len_card();
        }
    }
}

// ──────────────────────────────────────────────
// 光圈挡位选择事件回调
// ──────────────────────────────────────────────
static void aperture_step_event_cb(lv_event_t * e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    len_card_params_t *params = (len_card_params_t *)lv_event_get_user_data(e);

    uint32_t selected = lv_dropdown_get_selected(dropdown);
    params->current_step_type = (int)selected;

    // 根据挡位类型更新光圈选项
    const float *aperture_array;
    int count;
    int stride = 1;

    if (selected == 0) {
        // 1档 - 使用APERTURES_1_3表，步长为3
        aperture_array = APERTURES_1_3;
        count = COUNT_APERTURES_1_3;
        stride = 3;
    } else if (selected == 1) {
        // 1/2档
        aperture_array = APERTURES_1_2;
        count = COUNT_APERTURES_1_2;
        stride = 1;
    } else if (selected == 2) {
        // 1/3档
        aperture_array = APERTURES_1_3;
        count = COUNT_APERTURES_1_3;
        stride = 1;
    } else {
        // 自定义光圈 - 显示自定义输入框，隐藏dropdown
        lv_obj_add_flag(params->dropdown_min_aperture, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(params->dropdown_max_aperture, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(params->textarea_custom_aperture, LV_OBJ_FLAG_HIDDEN);

        // 更新主界面
        update_main_ui_from_len_card();
        return;
    }

    // 非自定义模式 - 显示dropdown，隐藏自定义输入框
    lv_obj_clear_flag(params->dropdown_min_aperture, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(params->dropdown_max_aperture, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(params->textarea_custom_aperture, LV_OBJ_FLAG_HIDDEN);

    char *options = app_ui_generate_aperture_options(aperture_array, count, stride);

    // 更新最小光圈和最大光圈的选项
    lv_dropdown_set_options(params->dropdown_min_aperture, options);
    lv_dropdown_set_options(params->dropdown_max_aperture, options);

    // 重置选择到中间位置
    int actual_count = (count + stride - 1) / stride;
    lv_dropdown_set_selected(params->dropdown_min_aperture, 0);
    lv_dropdown_set_selected(params->dropdown_max_aperture, actual_count - 1);

    // 更新主界面
    update_main_ui_from_len_card();
}

// ──────────────────────────────────────────────
// 最小光圈选择事件回调
// ──────────────────────────────────────────────
static void min_aperture_event_cb(lv_event_t * e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    len_card_params_t *params = (len_card_params_t *)lv_event_get_user_data(e);

    uint32_t min_idx = lv_dropdown_get_selected(dropdown);
    uint32_t max_idx = lv_dropdown_get_selected(params->dropdown_max_aperture);

    // 验证：最小光圈不能超过最大光圈
    if (min_idx > max_idx) {
        // 如果最小光圈大于最大光圈，需要调整
        lv_dropdown_set_selected(dropdown, max_idx);
    }

    // 更新主界面
    update_main_ui_from_len_card();
}

// ──────────────────────────────────────────────
// 最大光圈选择事件回调
// ──────────────────────────────────────────────
static void max_aperture_event_cb(lv_event_t * e)
{
    lv_obj_t *dropdown = lv_event_get_target(e);
    len_card_params_t *params = (len_card_params_t *)lv_event_get_user_data(e);

    uint32_t max_idx = lv_dropdown_get_selected(dropdown);
    uint32_t min_idx = lv_dropdown_get_selected(params->dropdown_min_aperture);

    // 验证：最大光圈不能小于最小光圈
    if (max_idx < min_idx) {
        // 如果最大光圈小于最小光圈，需要调整
        lv_dropdown_set_selected(dropdown, min_idx);
    }

    // 更新主界面
    update_main_ui_from_len_card();
}

// ──────────────────────────────────────────────
// 自定义光圈输入框事件回调
// ──────────────────────────────────────────────
static void custom_aperture_event_cb(lv_event_t * e)
{
    lv_obj_t *textarea = lv_event_get_target(e);
    len_card_params_t *params = (len_card_params_t *)lv_event_get_user_data(e);

    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_READY || code == LV_EVENT_DEFOCUSED) {
        // 当用户完成输入或输入框失去焦点时，解析光圈值
        const char *text = lv_textarea_get_text(textarea);

        // 释放之前的自定义光圈数组
        if (params->custom_aperture_array) {
            free(params->custom_aperture_array);
            params->custom_aperture_array = NULL;
        }

        // 解析新的光圈值
        params->custom_aperture_array = parse_custom_aperture_string(text, &params->custom_aperture_count);

        // 更新主界面
        update_main_ui_from_len_card();
    }
}

// ──────────────────────────────────────────────
// 镜头卡片事件回调
// ──────────────────────────────────────────────
static void len_card_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *card = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED) {
        // 单击 → 选中
        if (len_selected_card != card) {
            if (len_selected_card) {
                lv_obj_set_style_bg_color(len_selected_card, lv_color_hex(0x1e1e2e), 0);
                lv_obj_set_style_border_color(len_selected_card, lv_color_hex(0x444466), 0);
                lv_obj_set_style_border_width(len_selected_card, 1, 0);
            }

            lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a3e), 0);
            lv_obj_set_style_border_color(card, lv_color_hex(0x4a90e2), 0);
            lv_obj_set_style_border_width(card, 2, 0);

            len_selected_card = card;

            // 更新主界面
            update_main_ui_from_len_card();
        }
    }
    else if (code == LV_EVENT_LONG_PRESSED) {
        // 长按 → 弹出确认对话框
        len_pending_delete_card = card;
        len_show_delete_confirm_dialog();
    }
}

// ──────────────────────────────────────────────
// 镜头长按弹出窗口
// ──────────────────────────────────────────────
static void len_long_press_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_LONG_PRESSED)
    {
        lv_obj_clear_flag(len_win_select, LV_OBJ_FLAG_HIDDEN);
    }
}

// ──────────────────────────────────────────────
// 镜头删除确认对话框
// ──────────────────────────────────────────────
static void len_show_delete_confirm_dialog(void)
{
    lv_obj_t *mbox = lv_obj_create(lv_layer_top());
    lv_obj_set_size(mbox, 300, 150);
    lv_obj_center(mbox);
    lv_obj_set_style_radius(mbox, 10, 0);
    lv_obj_set_style_bg_color(mbox, lv_color_black(), 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_pad_all(mbox, 20, 0);
    lv_obj_set_flex_flow(mbox, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(mbox, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_opa(mbox, LV_OPA_70, 0);

    lv_obj_t *title = lv_label_create(mbox);
    lv_label_set_text(title, "Confirm Delete");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);

    lv_obj_t *msg = lv_label_create(mbox);
    lv_label_set_text(msg, "Are you sure to delete this lens?");
    lv_obj_set_style_text_color(msg, lv_color_hex(0xcccccc), 0);

    lv_obj_t *btn_cont = lv_obj_create(mbox);
    lv_obj_set_size(btn_cont, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(btn_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(btn_cont, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(btn_cont, 0, 0);
    lv_obj_set_style_bg_opa(btn_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_cont, 0, 0);

    lv_obj_t *btn_cancel = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_cancel, 100, 36);
    lv_obj_set_style_radius(btn_cancel, 6, 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x666666), 0);
    lv_obj_set_style_bg_color(btn_cancel, lv_color_hex(0x555555), LV_STATE_PRESSED);

    lv_obj_t *lbl_cancel = lv_label_create(btn_cancel);
    lv_label_set_text(lbl_cancel, "Cancel");
    lv_obj_center(lbl_cancel);
    lv_obj_set_style_text_color(lbl_cancel, lv_color_white(), 0);

    lv_obj_t *btn_confirm = lv_btn_create(btn_cont);
    lv_obj_set_size(btn_confirm, 100, 36);
    lv_obj_set_style_radius(btn_confirm, 6, 0);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xe74c3c), 0);
    lv_obj_set_style_bg_color(btn_confirm, lv_color_hex(0xc0392b), LV_STATE_PRESSED);

    lv_obj_t *lbl_confirm = lv_label_create(btn_confirm);
    lv_label_set_text(lbl_confirm, "Yes");
    lv_obj_center(lbl_confirm);
    lv_obj_set_style_text_color(lbl_confirm, lv_color_white(), 0);

    lv_obj_add_event_cb(btn_cancel, len_delete_confirm_event_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_add_event_cb(btn_confirm, len_delete_confirm_event_cb, LV_EVENT_CLICKED, mbox);
}

// ──────────────────────────────────────────────
// 镜头删除确认回调
// ──────────────────────────────────────────────
static void len_delete_confirm_event_cb(lv_event_t * e)
{
    lv_obj_t *mbox = lv_event_get_user_data(e);
    lv_obj_t *btn = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_CLICKED) {
        const char *txt = lv_label_get_text(lv_obj_get_child(btn, 0));

        if (strcmp(txt, "Yes") == 0) {
            // 用户点击了确认删除
            if (len_pending_delete_card) {
                bool was_selected = (len_selected_card == len_pending_delete_card);

                lv_obj_t *parent = lv_obj_get_parent(len_pending_delete_card);
                if (!parent) {
                    lv_obj_del_async(mbox);
                    len_pending_delete_card = NULL;
                    return;
                }

                uint32_t child_cnt = lv_obj_get_child_cnt(parent);
                int32_t idx = -1;

                for (uint32_t i = 0; i < child_cnt; i++) {
                    if (lv_obj_get_child(parent, i) == len_pending_delete_card) {
                        idx = (int32_t)i;
                        break;
                    }
                }

                if (idx >= 0) {
                    lv_obj_t *next_card = NULL;

                    if (idx + 1 < (int32_t)child_cnt) {
                        next_card = lv_obj_get_child(parent, idx + 1);
                    }
                    else if (idx > 0) {
                        next_card = lv_obj_get_child(parent, idx - 1);
                    }

                    /* 保存当前滚动位置和父对象指针 */
                    len_saved_scroll_y = lv_obj_get_scroll_y(parent);
                    len_scroll_parent = parent;

                    // 隐藏键盘并清除焦点
                    lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
                    lv_keyboard_set_textarea(cam_keyboard, NULL);

                    // 清除被删除卡片上所有输入框的焦点
                    lv_obj_t *card_content = lv_obj_get_child(len_pending_delete_card, 0);
                    if (card_content) {
                        uint32_t content_child_cnt = lv_obj_get_child_cnt(card_content);
                        for (uint32_t i = 0; i < content_child_cnt; i++) {
                            lv_obj_t *child = lv_obj_get_child(card_content, i);
                            if (lv_obj_check_type(child, &lv_textarea_class)) {
                                lv_obj_clear_state(child, LV_STATE_FOCUSED);
                                lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                            }
                        }
                    }

                    // 使用异步删除，避免在渲染过程中删除对象
                    lv_obj_del_async(len_pending_delete_card);

                    // 删除后立即清除所有剩余卡片上输入框的焦点
                    uint32_t parent_child_cnt = lv_obj_get_child_cnt(parent);
                    for (uint32_t i = 0; i < parent_child_cnt; i++) {
                        lv_obj_t *card = lv_obj_get_child(parent, i);
                        if (card) {
                            lv_obj_t *content = lv_obj_get_child(card, 0);
                            if (content) {
                                uint32_t content_child_cnt = lv_obj_get_child_cnt(content);
                                for (uint32_t j = 0; j < content_child_cnt; j++) {
                                    lv_obj_t *child = lv_obj_get_child(content, j);
                                    if (lv_obj_check_type(child, &lv_textarea_class)) {
                                        lv_obj_clear_state(child, LV_STATE_FOCUSED);
                                        lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                                    }
                                }
                            }
                        }
                    }
                    
                    // 删除后再次清除所有输入框的焦点（防止 LVGL 自动聚焦）
                    for (uint32_t i = 0; i < parent_child_cnt; i++) {
                        lv_obj_t *card = lv_obj_get_child(parent, i);
                        if (card) {
                            lv_obj_t *content = lv_obj_get_child(card, 0);
                            if (content) {
                                uint32_t content_child_cnt = lv_obj_get_child_cnt(content);
                                for (uint32_t j = 0; j < content_child_cnt; j++) {
                                    lv_obj_t *child = lv_obj_get_child(content, j);
                                    if (lv_obj_check_type(child, &lv_textarea_class)) {
                                        lv_obj_clear_state(child, LV_STATE_FOCUSED);
                                        lv_obj_clear_state(child, LV_STATE_FOCUS_KEY);
                                    }
                                }
                            }
                        }
                    }

                    if (was_selected) {
                        len_selected_card = next_card;

                        if (len_selected_card) {
                            lv_obj_set_style_bg_color(len_selected_card, lv_color_hex(0x2a2a3e), 0);
                            lv_obj_set_style_border_color(len_selected_card, lv_color_hex(0x4a90e2), 0);
                            lv_obj_set_style_border_width(len_selected_card, 2, 0);
                        }
                    }

                    if (lv_obj_get_child_cnt(parent) == 0) {
                        len_selected_card = NULL;
                    }
                    
                    /* 创建定时器，在下一帧恢复滚动位置和更新主界面 */
                    lv_timer_t * timer = lv_timer_create(len_restore_scroll_timer_cb, 1, NULL);
                    lv_timer_set_repeat_count(timer, 1);
                }
            }
        }

        // 关闭对话框
        lv_obj_del_async(mbox);
        len_pending_delete_card = NULL;
    }
}

// ──────────────────────────────────────────────
// 镜头窗口关闭回调
// ──────────────────────────────────────────────
static void len_close_win_cb(lv_event_t * e)
{
    // 先收起键盘
    lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(num_keyboard, LV_OBJ_FLAG_HIDDEN);
    // 再关闭窗口
    lv_obj_add_flag(len_win_select, LV_OBJ_FLAG_HIDDEN);
}

// ──────────────────────────────────────────────
// 按钮回调（添加镜头）
// ──────────────────────────────────────────────
static void btn_add_len_event_cb(lv_event_t * e)
{
    if (!len_card_win_container) return;

    lv_obj_t *card = lv_obj_create(len_card_win_container);
    lv_obj_set_size(card, LV_PCT(100), 130);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1e1e2e), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    // ── 新增：卡片的内容容器 ──
    lv_obj_t *card_content = lv_obj_create(card);
    lv_obj_remove_style_all(card_content);
    lv_obj_set_size(card_content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(card_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card_content, 0, 0);
    lv_obj_set_style_pad_all(card_content, 0, 0);
    lv_obj_set_flex_flow(card_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card_content, 8, 0);
    lv_obj_add_flag(card_content, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(card_content, LV_OBJ_FLAG_CLICKABLE);

    // 第一行：镜头名字和光圈挡位
    lv_obj_t *row1 = lv_obj_create(card_content);
    lv_obj_set_size(row1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row1, 8, 0);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_CLICKABLE);

    // 第二行：最小光圈、最大光圈和焦距
    lv_obj_t *row2 = lv_obj_create(card_content);
    lv_obj_set_size(row2, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row2, 8, 0);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_CLICKABLE);

    // 镜头参数卡片
    // 1. 镜头名字输入框（第一行）
    lv_obj_t* len_ta_name = lv_textarea_create(row1);
    lv_obj_set_size(len_ta_name, 120, 55);
    lv_textarea_set_placeholder_text(len_ta_name, "Len Name");
    lv_textarea_set_one_line(len_ta_name, true);
    lv_obj_set_style_text_font(len_ta_name, &lv_font_montserrat_14, 0);

    // 关联键盘
    lv_keyboard_set_textarea(cam_keyboard, len_ta_name);
    lv_obj_set_style_bg_color(len_ta_name, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(len_ta_name, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(len_ta_name, 1, 0);
    lv_obj_set_style_radius(len_ta_name, 4, 0);

    // 添加事件回调，点击时弹出键盘
    lv_obj_add_event_cb(len_ta_name, len_ta_name_event_cb, LV_EVENT_CLICKED, NULL);

    // 2. 光圈挡位 dropdown（第一行）
    lv_obj_t* dropdown_aperture_step = lv_dropdown_create(row1);
    lv_obj_set_size(dropdown_aperture_step, 90, 45);
    lv_dropdown_set_options(dropdown_aperture_step, "1\n1/2\n1/3\nCustom");
    lv_obj_set_style_text_font(dropdown_aperture_step, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dropdown_aperture_step, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(dropdown_aperture_step, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(dropdown_aperture_step, 1, 0);
    lv_obj_set_style_radius(dropdown_aperture_step, 4, 0);
    lv_obj_set_style_text_align(dropdown_aperture_step, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // 3. 最小光圈 dropdown（第二行）
    lv_obj_t* dropdown_min_aperture = lv_dropdown_create(row2);
    lv_obj_set_size(dropdown_min_aperture, 90, 45);
    lv_obj_set_style_text_font(dropdown_min_aperture, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dropdown_min_aperture, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(dropdown_min_aperture, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(dropdown_min_aperture, 1, 0);
    lv_obj_set_style_radius(dropdown_min_aperture, 4, 0);
    lv_obj_set_style_text_align(dropdown_min_aperture, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // 4. 最大光圈 dropdown（第二行）
    lv_obj_t* dropdown_max_aperture = lv_dropdown_create(row2);
    lv_obj_set_size(dropdown_max_aperture, 90, 45);
    lv_obj_set_style_text_font(dropdown_max_aperture, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dropdown_max_aperture, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(dropdown_max_aperture, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(dropdown_max_aperture, 1, 0);
    lv_obj_set_style_radius(dropdown_max_aperture, 4, 0);
    lv_obj_set_style_text_align(dropdown_max_aperture, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // 5. 焦距输入框（第二行）
    lv_obj_t* textarea_focal_length = lv_textarea_create(row2);
    lv_obj_set_size(textarea_focal_length, 90, 55);
    lv_textarea_set_placeholder_text(textarea_focal_length, "50mm");
    lv_textarea_set_one_line(textarea_focal_length, true);
    lv_obj_set_style_text_font(textarea_focal_length, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(textarea_focal_length, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(textarea_focal_length, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(textarea_focal_length, 1, 0);
    lv_obj_set_style_radius(textarea_focal_length, 4, 0);

    // 添加事件回调，点击时弹出数字键盘
    lv_obj_add_event_cb(textarea_focal_length, num_ta_event_cb, LV_EVENT_CLICKED, NULL);

    // 6. 自定义光圈输入框（默认隐藏，放在第二行末尾）
    lv_obj_t* textarea_custom_aperture = lv_textarea_create(row2);
    lv_obj_set_size(textarea_custom_aperture, 220, 55);
    lv_textarea_set_placeholder_text(textarea_custom_aperture, "1.4,2,2.8,4");
    lv_textarea_set_one_line(textarea_custom_aperture, true);
    lv_obj_set_style_text_font(textarea_custom_aperture, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(textarea_custom_aperture, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(textarea_custom_aperture, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(textarea_custom_aperture, 1, 0);
    lv_obj_set_style_radius(textarea_custom_aperture, 4, 0);
    lv_obj_add_flag(textarea_custom_aperture, LV_OBJ_FLAG_HIDDEN);  // 默认隐藏

    // 添加事件回调，点击时弹出数字键盘（带逗号）
    lv_obj_add_event_cb(textarea_custom_aperture, custom_aperture_ta_event_cb, LV_EVENT_CLICKED, NULL);

    // 创建参数数据结构并绑定事件
    len_card_params_t *params = (len_card_params_t *)malloc(sizeof(len_card_params_t));
    params->dropdown_aperture_step = dropdown_aperture_step;
    params->dropdown_min_aperture = dropdown_min_aperture;
    params->dropdown_max_aperture = dropdown_max_aperture;
    params->textarea_custom_aperture = textarea_custom_aperture;
    params->textarea_focal_length = textarea_focal_length;
    params->custom_aperture_array = NULL;
    params->custom_aperture_count = 0;
    params->current_step_type = 0;  // 默认 1 档

    // 初始化光圈选项（默认1档，使用步长3）
    char *options = app_ui_generate_aperture_options(APERTURES_1_3, COUNT_APERTURES_1_3, 3);
    lv_dropdown_set_options(dropdown_min_aperture, options);
    lv_dropdown_set_options(dropdown_max_aperture, options);

    // 设置默认值：最小光圈选最小的（数组开头），最大光圈选最大的（数组末尾）
    int actual_count = (COUNT_APERTURES_1_3 + 3 - 1) / 3;
    lv_dropdown_set_selected(dropdown_min_aperture, 0);
    lv_dropdown_set_selected(dropdown_max_aperture, actual_count - 1);

    // 绑定事件回调
    lv_obj_add_event_cb(dropdown_aperture_step, aperture_step_event_cb, LV_EVENT_VALUE_CHANGED, params);
    lv_obj_add_event_cb(dropdown_min_aperture, min_aperture_event_cb, LV_EVENT_VALUE_CHANGED, params);
    lv_obj_add_event_cb(dropdown_max_aperture, max_aperture_event_cb, LV_EVENT_VALUE_CHANGED, params);
    lv_obj_add_event_cb(textarea_custom_aperture, custom_aperture_event_cb, LV_EVENT_ALL, params);

    // 将params指针存储到card的user_data中，以便在update_main_ui_from_len_card中访问
    lv_obj_set_user_data(card, params);

    // 绑定点击 + 长按（目标仍是 card）
    lv_obj_add_event_cb(card, len_card_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card, len_card_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    // 自动选中第一张
    if (lv_obj_get_child_cnt(len_card_win_container) == 1) {
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a3e), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x4a90e2), 0);
        lv_obj_set_style_border_width(card, 2, 0);
        len_selected_card = card;
    }

    lv_obj_scroll_to_view(card, LV_ANIM_ON);
}

// ──────────────────────────────────────────────
// 从选中的镜头卡片更新主界面
// ──────────────────────────────────────────────
static void update_main_ui_from_len_card(void)
{
    if (!len_selected_card) {
        // 没有选中的卡片，显示默认值
        if (main_label_len) {
            lv_label_set_text(main_label_len, "----");
        }
        if (main_roller_aperture) {
            // 显示默认的1档光圈（使用步长3）
            char *options = app_ui_generate_aperture_options(APERTURES_1_3, COUNT_APERTURES_1_3, 3);
            lv_roller_set_options(main_roller_aperture, options, LV_ROLLER_MODE_NORMAL);
        }
        return;
    }

    // 获取卡片的内容容器
    lv_obj_t *card_content = lv_obj_get_child(len_selected_card, 0);
    if (!card_content) {
        return;
    }

    // 获取第一行容器（第1个子对象）
    lv_obj_t *row1 = lv_obj_get_child(card_content, 0);
    // 获取第二行容器（第2个子对象）
    lv_obj_t *row2 = lv_obj_get_child(card_content, 1);

    if (!row1 || !row2) {
        return;
    }

    // 获取镜头名称输入框（row1的第1个子对象）
    lv_obj_t *len_ta_name = lv_obj_get_child(row1, 0);
    if (len_ta_name) {
        const char *name = lv_textarea_get_text(len_ta_name);
        if (name && strlen(name) > 0) {
            lv_label_set_text(main_label_len, name);
        } else {
            lv_label_set_text(main_label_len, "----");
        }
    }

    // 获取光圈挡位下拉框（row1的第2个子对象）
    lv_obj_t *dropdown_aperture_step = lv_obj_get_child(row1, 1);
    // 获取最小光圈下拉框（row2的第1个子对象）
    lv_obj_t *dropdown_min_aperture = lv_obj_get_child(row2, 0);
    // 获取最大光圈下拉框（row2的第2个子对象）
    lv_obj_t *dropdown_max_aperture = lv_obj_get_child(row2, 1);
    // 获取自定义光圈输入框（row2的第4个子对象，默认隐藏）
    lv_obj_t *textarea_custom_aperture = lv_obj_get_child(row2, 3);

    // 从card的user_data中获取params
    len_card_params_t *params = (len_card_params_t *)lv_obj_get_user_data(len_selected_card);

    if (dropdown_aperture_step) {
        // 获取当前选中的挡位类型
        uint32_t step_type = lv_dropdown_get_selected(dropdown_aperture_step);

        // 检查是否是自定义光圈模式
        if (step_type == 3 && params && params->custom_aperture_array && params->custom_aperture_count > 0) {
            // 自定义光圈模式 - 使用自定义光圈数组更新主界面
            if (main_roller_aperture) {
                // 生成光圈选项字符串
                static char options[1024];
                int pos = 0;

                for (int i = 0; i < params->custom_aperture_count && pos < sizeof(options) - 20; i++) {
                    char buf[32];
                    format_aperture_string(buf, sizeof(buf), params->custom_aperture_array[i]);
                    int len = strlen(buf);
                    if (pos + len + 2 > sizeof(options)) break;
                    if (i > 0) {
                        options[pos++] = '\n';
                    }
                    strcpy(&options[pos], buf);
                    pos += len;
                }
                options[pos] = '\0';

                // 更新主界面的光圈滚轮
                lv_roller_set_options(main_roller_aperture, options, LV_ROLLER_MODE_NORMAL);
            }
            return;
        }

        if (dropdown_min_aperture && dropdown_max_aperture) {
            // 获取最小和最大光圈的索引（dropdown中的索引）
            uint32_t min_idx = lv_dropdown_get_selected(dropdown_min_aperture);
            uint32_t max_idx = lv_dropdown_get_selected(dropdown_max_aperture);

            // 根据挡位类型选择源表和步长
            const float *aperture_array;
            int count;
            int stride = 1;

            if (step_type == 0) {
                // 1档
                aperture_array = APERTURES_1_3;
                count = COUNT_APERTURES_1_3;
                stride = 3;
            } else if (step_type == 1) {
                // 1/2档
                aperture_array = APERTURES_1_2;
                count = COUNT_APERTURES_1_2;
                stride = 1;
            } else {
                // 1/3档
                aperture_array = APERTURES_1_3;
                count = COUNT_APERTURES_1_3;
                stride = 1;
            }

            // 将dropdown索引转换为原始数组索引
            // dropdown中的索引需要乘以步长才能得到原始数组的索引
            int orig_min_idx = (int)min_idx * stride;
            int orig_max_idx = (int)max_idx * stride;

            // 生成光圈选项字符串（从orig_min_idx到orig_max_idx，按步长stride）
            static char options[1024];
            int pos = 0;

            for (int i = orig_min_idx; i <= orig_max_idx && i >= 0 && i < count && pos < sizeof(options) - 20; i += stride) {
                char buf[32];
                format_aperture_string(buf, sizeof(buf), aperture_array[i]);
                int len = strlen(buf);
                if (pos + len + 2 > sizeof(options)) break;
                if (i != orig_min_idx) {
                    options[pos++] = '\n';
                }
                strcpy(&options[pos], buf);
                pos += len;
            }
            options[pos] = '\0';

            // 更新主界面的光圈滚轮
            lv_roller_set_options(main_roller_aperture, options, LV_ROLLER_MODE_NORMAL);
        }
    }
}

static float parse_focal_length_value(const char* str)
{
    if (!str || strlen(str) == 0) {
        return 50.0f;
    }

    return atof(str);
}

static LEN extract_len_from_card(lv_obj_t* card)
{
    LEN len = {0};
    len.aperture_stops = NULL;
    len.aperture_stop_count = 0;
    len.focal_length = 0.0f;  // 0 表示无自定义焦距

    if (!card) {
        return len;
    }

    lv_obj_t *card_content = lv_obj_get_child(card, 0);
    if (!card_content) {
        return len;
    }

    // 获取第一行容器（第1个子对象）
    lv_obj_t *row1 = lv_obj_get_child(card_content, 0);
    // 获取第二行容器（第2个子对象）
    lv_obj_t *row2 = lv_obj_get_child(card_content, 1);

    if (!row1 || !row2) {
        return len;
    }

    len_card_params_t *params = (len_card_params_t *)lv_obj_get_user_data(card);
    lv_obj_t *dropdown_aperture_step = lv_obj_get_child(row1, 1);
    lv_obj_t *dropdown_min_aperture = lv_obj_get_child(row2, 0);
    lv_obj_t *dropdown_max_aperture = lv_obj_get_child(row2, 1);
    lv_obj_t *textarea_focal_length = lv_obj_get_child(row2, 2);

    if (dropdown_aperture_step) {
        uint32_t step_type = lv_dropdown_get_selected(dropdown_aperture_step);

        if (step_type == 3 && params && params->custom_aperture_array && params->custom_aperture_count > 0) {
            len.aperture_stop_count = params->custom_aperture_count;
            len.aperture_stops = (float*)malloc(params->custom_aperture_count * sizeof(float));
            if (len.aperture_stops) {
                memcpy(len.aperture_stops, params->custom_aperture_array,
                       params->custom_aperture_count * sizeof(float));
            }
            return len;
        }

        if (dropdown_min_aperture && dropdown_max_aperture) {
            uint32_t min_idx = lv_dropdown_get_selected(dropdown_min_aperture);
            uint32_t max_idx = lv_dropdown_get_selected(dropdown_max_aperture);

            const float *aperture_array;
            int count;
            int stride = 1;

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

            int orig_min_idx = (int)min_idx * stride;
            int orig_max_idx = (int)max_idx * stride;

            int actual_count = 0;
            for (int i = orig_min_idx; i <= orig_max_idx && i >= 0 && i < count; i += stride) {
                actual_count++;
            }

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

    if (textarea_focal_length) {
        const char* focal_length_str = lv_textarea_get_text(textarea_focal_length);
        if (focal_length_str && strlen(focal_length_str) > 0) {
            len.focal_length = parse_focal_length_value(focal_length_str);
        }
    }

    return len;
}

// ──────────────────────────────────────────────
// 添加相机按钮事件回调
// ──────────────────────────────────────────────
static void btn_add_cam_event_cb(lv_event_t * e)
{
    if (!cam_card_win_container) return;

    lv_obj_t *card = lv_obj_create(cam_card_win_container);
    lv_obj_set_size(card, LV_PCT(100), 130);
    lv_obj_set_style_radius(card, 8, 0);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1e1e2e), 0);
    lv_obj_set_style_border_color(card, lv_color_hex(0x444466), 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_pad_all(card, 12, 0);

    // ── 新增：卡片的内容容器 ──
    lv_obj_t *card_content = lv_obj_create(card);
    lv_obj_remove_style_all(card_content);           // 去掉默认样式
    lv_obj_set_size(card_content, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_opa(card_content, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(card_content, 0, 0);
    lv_obj_set_style_pad_all(card_content, 0, 0);
    lv_obj_set_flex_flow(card_content, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card_content, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(card_content, 8, 0);
    lv_obj_add_flag(card_content, LV_OBJ_FLAG_EVENT_BUBBLE);
    lv_obj_clear_flag(card_content, LV_OBJ_FLAG_CLICKABLE);

    // 第一行：相机名字和快门挡位
    lv_obj_t *row1 = lv_obj_create(card_content);
    lv_obj_set_size(row1, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row1, 8, 0);
    lv_obj_set_style_bg_opa(row1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row1, 0, 0);
    lv_obj_set_style_pad_all(row1, 0, 0);
    lv_obj_clear_flag(row1, LV_OBJ_FLAG_CLICKABLE);

    // 第二行：最小快门、最大快门和闪光同步
    lv_obj_t *row2 = lv_obj_create(card_content);
    lv_obj_set_size(row2, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(row2, 8, 0);
    lv_obj_set_style_bg_opa(row2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row2, 0, 0);
    lv_obj_set_style_pad_all(row2, 0, 0);
    lv_obj_clear_flag(row2, LV_OBJ_FLAG_CLICKABLE);

    // 相机参数卡片
    // 1. 相机名字输入框（第一行）
    lv_obj_t* cam_ta_name = lv_textarea_create(row1);
    lv_obj_set_size(cam_ta_name, 120, 55);
    lv_textarea_set_placeholder_text(cam_ta_name, "Cam Name");
    lv_textarea_set_one_line(cam_ta_name, true);
    lv_obj_set_style_text_font(cam_ta_name, &lv_font_montserrat_14, 0);

    // 关联键盘
    lv_keyboard_set_textarea(cam_keyboard, cam_ta_name);
    lv_obj_set_style_bg_color(cam_ta_name, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(cam_ta_name, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(cam_ta_name, 1, 0);
    lv_obj_set_style_radius(cam_ta_name, 4, 0);


    // 添加事件回调，点击时弹出键盘
    lv_obj_add_event_cb(cam_ta_name, cam_ta_name_event_cb, LV_EVENT_CLICKED, NULL);

    // 2. 快门挡位 dropdown（第一行）
    lv_obj_t* dropdown_shutter_step = lv_dropdown_create(row1);
    lv_obj_set_size(dropdown_shutter_step, 90, 45);
    lv_dropdown_set_options(dropdown_shutter_step, "1\n1/2\n1/3");
    lv_obj_set_style_text_font(dropdown_shutter_step, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dropdown_shutter_step, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(dropdown_shutter_step, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(dropdown_shutter_step, 1, 0);
    lv_obj_set_style_radius(dropdown_shutter_step, 4, 0);
    lv_obj_set_style_text_align(dropdown_shutter_step, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // 3. 最小快门 dropdown（第二行）
    lv_obj_t* dropdown_min_shutter = lv_dropdown_create(row2);
    lv_obj_set_size(dropdown_min_shutter, 90, 45);
    lv_obj_set_style_text_font(dropdown_min_shutter, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dropdown_min_shutter, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(dropdown_min_shutter, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(dropdown_min_shutter, 1, 0);
    lv_obj_set_style_radius(dropdown_min_shutter, 4, 0);
    lv_obj_set_style_text_align(dropdown_min_shutter, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // 4. 最大快门 dropdown（第二行）
    lv_obj_t* dropdown_max_shutter = lv_dropdown_create(row2);
    lv_obj_set_size(dropdown_max_shutter, 90, 45);
    lv_obj_set_style_text_font(dropdown_max_shutter, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(dropdown_max_shutter, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(dropdown_max_shutter, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(dropdown_max_shutter, 1, 0);
    lv_obj_set_style_radius(dropdown_max_shutter, 4, 0);
    lv_obj_set_style_text_align(dropdown_max_shutter, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    // 5. 闪光同步值输入框（第二行）
    lv_obj_t* textarea_flash_sync = lv_textarea_create(row2);
    lv_obj_set_size(textarea_flash_sync, 90, 55);
    lv_textarea_set_placeholder_text(textarea_flash_sync, "1/250");
    lv_textarea_set_one_line(textarea_flash_sync, true);
    lv_obj_set_style_text_font(textarea_flash_sync, &lv_font_montserrat_14, 0);
    lv_obj_set_style_bg_color(textarea_flash_sync, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_border_color(textarea_flash_sync, lv_color_hex(0x4a90e2), 0);
    lv_obj_set_style_border_width(textarea_flash_sync, 1, 0);
    lv_obj_set_style_radius(textarea_flash_sync, 4, 0);

    // 添加事件回调，点击时弹出数字键盘
    lv_obj_add_event_cb(textarea_flash_sync, num_ta_event_cb, LV_EVENT_CLICKED, NULL);

    // 创建参数数据结构并绑定事件
    cam_card_params_t *params = (cam_card_params_t *)malloc(sizeof(cam_card_params_t));
    params->dropdown_shutter_step = dropdown_shutter_step;
    params->dropdown_min_shutter = dropdown_min_shutter;
    params->dropdown_max_shutter = dropdown_max_shutter;
    params->textarea_flash_sync = textarea_flash_sync;
    params->current_step_type = 0;  // 默认 1 档

    // 初始化快门选项（默认1档，使用步长3）
    char *options = app_ui_generate_shutter_options(SHUTTERS_1_3, COUNT_SHUTTERS_1_3, 3);
    lv_dropdown_set_options(dropdown_min_shutter, options);
    lv_dropdown_set_options(dropdown_max_shutter, options);

    // 设置默认值：最小快门选最慢的（数组末尾），最大快门选最快的（数组开头）
    int actual_count = (COUNT_SHUTTERS_1_3 + 3 - 1) / 3;
    lv_dropdown_set_selected(dropdown_min_shutter, actual_count - 1);
    lv_dropdown_set_selected(dropdown_max_shutter, 0);

    // 绑定事件回调
    lv_obj_add_event_cb(dropdown_shutter_step, shutter_step_event_cb, LV_EVENT_VALUE_CHANGED, params);
    lv_obj_add_event_cb(dropdown_min_shutter, min_shutter_event_cb, LV_EVENT_VALUE_CHANGED, params);
    lv_obj_add_event_cb(dropdown_max_shutter, max_shutter_event_cb, LV_EVENT_VALUE_CHANGED, params);


    // 绑定点击 + 长按（目标仍是 card）
    lv_obj_add_event_cb(card, cam_card_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(card, cam_card_event_cb, LV_EVENT_LONG_PRESSED, NULL);

    // 自动选中第一张
    if (lv_obj_get_child_cnt(cam_card_win_container) == 1) {
        lv_obj_set_style_bg_color(card, lv_color_hex(0x2a2a3e), 0);
        lv_obj_set_style_border_color(card, lv_color_hex(0x4a90e2), 0);
        lv_obj_set_style_border_width(card, 2, 0);
        cam_selected_card = card;
    }

    lv_obj_scroll_to_view(card, LV_ANIM_ON);
}


// ──────────────────────────────────────────────
// UI 初始化主函数
// ──────────────────────────────────────────────
void ui_exposure_init(void) {

    /* 根据屏幕宽度设置字体 */
    if (scr_act_width() <= 480)
    {
        font = &lv_font_montserrat_14;
    }
    else
    {
        font = &lv_font_montserrat_20;
    }

    lv_obj_t* tileview = lv_tileview_create(lv_scr_act());
    lv_obj_set_style_bg_color(tileview, lv_color_black(), 0);
    lv_obj_t* tile_main = lv_tileview_add_tile(tileview, 0, 1, LV_DIR_TOP);
    lv_obj_t* tile_setting = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_BOTTOM);

    lv_obj_set_tile_id(tileview, 0, 1, LV_ANIM_OFF);
    
    //--------------------------------------------------------+
    ui_setting_page_init(tile_setting);     
    //--------------------------------------------------------+

    /* 顶部状态栏布局  flex布局 */
    lv_obj_t* main_flex_layout = lv_obj_create(tile_main);
    lv_obj_remove_style_all(main_flex_layout);
    lv_obj_align(main_flex_layout, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_size(main_flex_layout, lv_pct(96), lv_pct(5));
    lv_obj_set_flex_flow(main_flex_layout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_flex_layout, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);   // 两端对齐

    /* 左侧状态栏 */
    main_table_time = lv_label_create(main_flex_layout);
    lv_label_set_text(main_table_time, "AM 8:30");
    lv_obj_set_style_text_font(main_table_time, font, LV_STATE_DEFAULT);

    /* 右侧状态栏 */
    main_table_status = lv_label_create(main_flex_layout);
    lv_label_set_text(main_table_status, LV_SYMBOL_WIFI "   80% " LV_SYMBOL_BATTERY_3 );
    lv_obj_set_style_text_font(main_table_status, font, LV_STATE_DEFAULT);

    /* ────────────────────────────────────────────── */
    /* 主体网格布局 */
    /* ────────────────────────────────────────────── */
    lv_obj_t* main_grid_layout = lv_obj_create(tile_main);
    lv_obj_remove_style_all(main_grid_layout);
    lv_obj_align(main_grid_layout, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_size(main_grid_layout, LV_PCT(96), LV_PCT(93));
    lv_obj_set_style_border_width(main_grid_layout, 0, 0);
    lv_obj_set_style_pad_all(main_grid_layout, 0, 0);           // 去掉内边距，让子控件贴边

    lv_obj_set_layout(main_grid_layout, LV_LAYOUT_GRID);

    static const lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static const lv_coord_t row_dsc[] = {LV_GRID_FR(2), LV_GRID_FR(5), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_set_style_grid_column_dsc_array(main_grid_layout, col_dsc, 0);
    lv_obj_set_style_grid_row_dsc_array(main_grid_layout, row_dsc, 0);
    lv_obj_set_style_pad_row(main_grid_layout, 8, 0);           // 垂直方向 cell 间距（行间距）
    lv_obj_set_style_pad_column(main_grid_layout, 8, 0);        // 水平方向 cell 间距（列间距）

    /* 相机参数容器 */
    lv_obj_t* main_obj_cam = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_cam, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_radius(main_obj_cam, 15, 0);
    lv_obj_set_style_bg_color(main_obj_cam, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_cam, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_cam, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj_cam, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);   // 两端对齐

    lv_obj_set_style_pad_all(main_obj_cam, 2, 0);
    lv_obj_set_style_pad_column(main_obj_cam, 4, 0);  // 子对象之间的水平间距

    // 长按弹出窗口
    lv_obj_add_flag(main_obj_cam, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(main_obj_cam, cam_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

    // 装图片的容器对象
    lv_obj_t* main_obj_cam_img = lv_obj_create(main_obj_cam);
    lv_obj_set_width(main_obj_cam_img, lv_pct(50));
    lv_obj_set_height(main_obj_cam_img, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_cam_img, 3);   // 宽度按需拉伸

    lv_obj_set_style_bg_opa(main_obj_cam_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_cam_img, 0, 0);
    lv_obj_set_scrollbar_mode(main_obj_cam_img, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_cam_img, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // 装名字的容器对象
    lv_obj_t* main_obj_cam_name = lv_obj_create(main_obj_cam);
    lv_obj_set_width(main_obj_cam_name, lv_pct(50));
    lv_obj_set_height(main_obj_cam_name, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_cam_name, 4);

    lv_obj_set_style_bg_opa(main_obj_cam_name, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_cam_name, 0, 0);
    lv_obj_set_style_pad_all(main_obj_cam_name, 0, 0);         // 去掉所有内边距
    lv_obj_set_scrollbar_mode(main_obj_cam_name, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_cam_name, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // cam图片
    lv_obj_t* main_img_cam = lv_img_create(main_obj_cam_img);
    lv_img_set_src(main_img_cam, &img_cam);
    lv_img_set_zoom(main_img_cam, 140);
    lv_img_set_antialias(main_img_cam, true);
    lv_obj_set_style_img_recolor_opa(main_img_cam, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(main_img_cam, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(main_img_cam);

    // cam_name文字
    main_label_cam = lv_label_create(main_obj_cam_name);
    lv_obj_set_style_text_font(main_label_cam, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_cam, lv_pct(100));
    lv_label_set_long_mode(main_label_cam, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(main_label_cam, cam_name);
    lv_obj_center(main_label_cam);


    /* 镜头参数容器 */
    lv_obj_t* main_obj_len = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_len, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_radius(main_obj_len, 15, 0);
    lv_obj_set_style_bg_color(main_obj_len, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_len, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_len, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj_len, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);   // 两端对齐

    lv_obj_set_style_pad_all(main_obj_len, 2, 0);
    lv_obj_set_style_pad_column(main_obj_len, 4, 0);  // 子对象之间的水平间距

    // 长按弹出窗口
    lv_obj_add_flag(main_obj_len, LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_PRESS_LOCK);
    lv_obj_add_event_cb(main_obj_len, len_long_press_cb, LV_EVENT_LONG_PRESSED, NULL);

    // 装镜头的容器对象
    lv_obj_t* main_obj_len_img = lv_obj_create(main_obj_len);
    lv_obj_set_width(main_obj_len_img, lv_pct(50));
    lv_obj_set_height(main_obj_len_img, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_len_img, 3);   // 宽度按需拉伸

    lv_obj_set_style_bg_opa(main_obj_len_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_len_img, 0, 0);
    lv_obj_set_scrollbar_mode(main_obj_len_img, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_len_img, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // 装名字的容器对象
    lv_obj_t* main_obj_len_name = lv_obj_create(main_obj_len);
    lv_obj_set_width(main_obj_len_name, lv_pct(50));
    lv_obj_set_height(main_obj_len_name, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_len_name, 4);

    lv_obj_set_style_bg_opa(main_obj_len_name, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_len_name, 0, 0);
    lv_obj_set_style_pad_all(main_obj_len_name, 0, 0);         // 去掉所有内边距
    lv_obj_set_scrollbar_mode(main_obj_len_name, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_len_name, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    // len图片
    lv_obj_t* main_img_len = lv_img_create(main_obj_len_img);
    lv_img_set_src(main_img_len, &img_len);
    lv_img_set_zoom(main_img_len, 140);
    lv_img_set_antialias(main_img_len, true);
    lv_obj_set_style_img_recolor_opa(main_img_len, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(main_img_len, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(main_img_len);

    // len_name文字
    main_label_len = lv_label_create(main_obj_len_name);
    lv_obj_set_style_text_font(main_label_len, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_len, lv_pct(100));
    lv_label_set_long_mode(main_label_len, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(main_label_len, len_name);
    lv_obj_center(main_label_len);

    /* 快门速度 */
    lv_obj_t* main_obj_shutter = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_shutter, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_radius(main_obj_shutter, 15, 0);
    lv_obj_set_style_bg_color(main_obj_shutter, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_shutter, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_shutter, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_obj_shutter, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    lv_obj_set_style_pad_row(main_obj_shutter, 10, 0);  // 子对象垂直间距

    // Tv标签
    lv_obj_t* main_label_shutter = lv_label_create(main_obj_shutter);
    lv_label_set_text(main_label_shutter, "Tv");
    lv_obj_set_style_text_font(main_label_shutter, &lv_font_montserrat_22, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_shutter, lv_pct(100));
    lv_obj_set_style_text_align(main_label_shutter, LV_TEXT_ALIGN_CENTER, 0);  // 文字居中

    // Tv滚轮
    main_roller_shutter = lv_roller_create(main_obj_shutter);
    lv_roller_set_options(main_roller_shutter, roller_shutter_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_shutter, true);
    lv_obj_set_width(main_roller_shutter, lv_pct(95));     // 推荐 80%~95%
    lv_obj_set_style_text_align(main_roller_shutter, LV_TEXT_ALIGN_CENTER, 0);  // 文字居中
    lv_obj_add_event_cb(main_roller_shutter, roller_manual_mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* 镜头光圈 */
    lv_obj_t* main_obj_aperture = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_aperture, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_radius(main_obj_aperture, 15, 0);
    lv_obj_set_style_bg_color(main_obj_aperture, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_aperture, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_aperture, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_obj_aperture, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    lv_obj_set_style_pad_row(main_obj_aperture, 10, 0);  // 子对象垂直间距

    // Av标签
    lv_obj_t* main_label_aperture = lv_label_create(main_obj_aperture);
    lv_label_set_text(main_label_aperture, "Av");
    lv_obj_set_style_text_font(main_label_aperture, &lv_font_montserrat_22, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_aperture, lv_pct(100));
    lv_obj_set_style_text_align(main_label_aperture, LV_TEXT_ALIGN_CENTER, 0);  // 文字居中

    // Av滚轮
    main_roller_aperture = lv_roller_create(main_obj_aperture);
    lv_roller_set_options(main_roller_aperture, roller_aperture_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_aperture, true);
    lv_obj_set_width(main_roller_aperture, lv_pct(95));     // 推荐 80%~95%
    lv_obj_set_style_text_align(main_roller_aperture, LV_TEXT_ALIGN_CENTER, 0);  // 文字居中
    lv_obj_add_event_cb(main_roller_aperture, roller_manual_mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    /* ISO选择 */
    lv_obj_t* main_obj_iso = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_iso, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
    lv_obj_set_style_radius(main_obj_iso, 15, 0);
    lv_obj_set_style_bg_color(main_obj_iso, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_iso, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_iso, LV_FLEX_FLOW_ROW);

    // 主轴水平居中，交叉轴（垂直）居中
    lv_obj_set_flex_align(main_obj_iso,
                           LV_FLEX_ALIGN_CENTER,       // 水平整体居中
                           LV_FLEX_ALIGN_CENTER,       // 垂直居中 ← 关键
                           LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_column(main_obj_iso, 30, 0);   // 标签和滚轮间距，20~40 自己调
    lv_obj_set_style_pad_top(main_obj_iso, 0, 0);
    lv_obj_set_style_pad_bottom(main_obj_iso, 0, 0);
    lv_obj_set_style_pad_left(main_obj_iso, 10, 0);     // 可选：轻微左右平衡
    lv_obj_set_style_pad_right(main_obj_iso, 10, 0);

    // 强制关闭滚动（预防）
    lv_obj_set_scrollbar_mode(main_obj_iso, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_iso, LV_OBJ_FLAG_SCROLLABLE);

    // ISO文字标签
    lv_obj_t* main_label_iso = lv_label_create(main_obj_iso);
    lv_label_set_text(main_label_iso, "ISO");
    lv_obj_set_style_text_font(main_label_iso, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_iso, LV_SIZE_CONTENT);          // 只占文字宽度
    lv_obj_set_style_text_align(main_label_iso, LV_TEXT_ALIGN_CENTER, 0);   // 让 label 内部文本垂直居中（如果有多行也有效）
    lv_obj_set_style_align(main_label_iso, LV_ALIGN_CENTER, 0); // 或 lv_obj_center(main_label_iso); 但优先用 style

    // ISO 滚轮
    main_roller_iso = lv_roller_create(main_obj_iso);
    lv_roller_set_options(main_roller_iso, roller_iso_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_iso, false);
    lv_obj_set_width(main_roller_iso, lv_pct(55));          // 调整到合适比例，50~65%
    lv_obj_set_height(main_roller_iso, lv_pct(100));        // 保持占满
    lv_obj_set_style_text_align(main_roller_iso, LV_TEXT_ALIGN_CENTER, 0);
    lv_roller_set_selected(main_roller_iso, 1, 0);          // 默认iso 100

    /* EV */
    lv_obj_t* main_obj_ev = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_ev, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
    lv_obj_set_style_radius(main_obj_ev, 15, 0);
    lv_obj_set_style_bg_color(main_obj_ev, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_ev, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_ev, LV_FLEX_FLOW_ROW);

    // 主轴水平居中，交叉轴（垂直）居中
    lv_obj_set_flex_align(main_obj_ev,
                           LV_FLEX_ALIGN_CENTER,       // 水平整体居中
                           LV_FLEX_ALIGN_CENTER,       // 垂直居中 ← 关键
                           LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_column(main_obj_ev, 30, 0);   // 标签和滚轮间距，20~40 自己调
    lv_obj_set_style_pad_top(main_obj_ev, 0, 0);
    lv_obj_set_style_pad_bottom(main_obj_ev, 0, 0);
    lv_obj_set_style_pad_left(main_obj_ev, 10, 0);     // 可选：轻微左右平衡
    lv_obj_set_style_pad_right(main_obj_ev, 10, 0);

    // 强制关闭滚动（预防）
    lv_obj_set_scrollbar_mode(main_obj_ev, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_ev, LV_OBJ_FLAG_SCROLLABLE);

    // ev文字标签
    lv_obj_t* main_label_ev = lv_label_create(main_obj_ev);
    lv_label_set_text(main_label_ev, "EV");
    lv_obj_set_style_text_font(main_label_ev, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_ev, LV_SIZE_CONTENT);          // 只占文字宽度
    lv_obj_set_style_text_align(main_label_ev, LV_TEXT_ALIGN_CENTER, 0);   // 让 label 内部文本垂直居中（如果有多行也有效）
    lv_obj_set_style_align(main_label_ev, LV_ALIGN_CENTER, 0); // 或 lv_obj_center(main_label_ev); 但优先用 style

    // ev 滚轮
    main_roller_ev = lv_roller_create(main_obj_ev);
    lv_roller_set_options(main_roller_ev, roller_ev_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_ev, false);
    lv_obj_set_width(main_roller_ev, lv_pct(55));          // 调整到合适比例，50~65%
    lv_obj_set_height(main_roller_ev, lv_pct(100));        // 保持占满
    lv_obj_set_style_text_align(main_roller_ev, LV_TEXT_ALIGN_CENTER, 0);
    lv_roller_set_selected(main_roller_ev, 4, 0);           // 默认ev 0

    /* 模式选择 */
    lv_obj_t* main_obj_mode = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_mode, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_set_style_radius(main_obj_mode, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(main_obj_mode, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_mode, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_mode, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj_mode, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_column(main_obj_mode, 5, 0);   // 标签和滚轮间距，20~40 自己调
    lv_obj_set_style_pad_top(main_obj_mode, 0, 0);
    lv_obj_set_style_pad_bottom(main_obj_mode, 0, 0);
    lv_obj_set_style_pad_left(main_obj_mode, 10, 0);     // 可选：轻微左右平衡
    lv_obj_set_style_pad_right(main_obj_mode, 10, 0);

    // 强制关闭滚动（预防）
    lv_obj_set_scrollbar_mode(main_obj_mode, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_mode, LV_OBJ_FLAG_SCROLLABLE);

    // mode文字标签
    lv_obj_t* main_label_mode = lv_label_create(main_obj_mode);
    lv_label_set_text(main_label_mode, "Mode");
    lv_obj_set_style_text_font(main_label_mode, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_mode, LV_SIZE_CONTENT);          // 只占文字宽度
    lv_obj_set_style_text_align(main_label_mode, LV_TEXT_ALIGN_CENTER, 0);   // 让 label 内部文本垂直居中（如果有多行也有效）
    lv_obj_set_style_align(main_label_mode, LV_ALIGN_CENTER, 0); // 或 lv_obj_center(main_label_mode); 但优先用 style

    // 选择
    main_obj_mode_select = lv_obj_create(main_obj_mode);
    lv_obj_set_width(main_obj_mode_select, lv_pct(70));          // 调整到合适比例，50~65%
    lv_obj_set_height(main_obj_mode_select, lv_pct(100));        // 保持占满

    create_mode_selector(main_obj_mode_select);

    /* lux 显示*/
    lv_obj_t* main_obj_lux = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_lux, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_set_style_radius(main_obj_lux, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(main_obj_lux, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(main_obj_lux, LV_OPA_COVER, 0);
    lv_obj_set_flex_flow(main_obj_lux, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj_lux, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_column(main_obj_lux, 30, 0);   // 标签和滚轮间距，20~40 自己调
    lv_obj_set_style_pad_top(main_obj_lux, 0, 0);
    lv_obj_set_style_pad_bottom(main_obj_lux, 0, 0);
    lv_obj_set_style_pad_left(main_obj_lux, 10, 0);     // 可选：轻微左右平衡
    lv_obj_set_style_pad_right(main_obj_lux, 10, 0);

    // 强制关闭滚动（预防）
    lv_obj_set_scrollbar_mode(main_obj_lux, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_lux, LV_OBJ_FLAG_SCROLLABLE);

    // Lux
    lv_obj_t* main_label_lux = lv_label_create(main_obj_lux);
    lv_label_set_text(main_label_lux, "Lux");
    lv_obj_set_style_text_font(main_label_lux, &lv_font_montserrat_20, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_lux, LV_SIZE_CONTENT);          // 只占文字宽度
    lv_obj_set_style_text_align(main_label_lux, LV_TEXT_ALIGN_CENTER, 0);   // 让 label 内部文本垂直居中（如果有多行也有效）
    lv_obj_set_style_align(main_label_lux, LV_ALIGN_CENTER, 0); // 或 lv_obj_center(main_label_ev); 但优先用 style

    // lux数值显示
    main_label_lux_value = lv_label_create(main_obj_lux);
    lv_obj_set_width(main_label_lux_value, lv_pct(55));          // 调整到合适比例，50~65%
    lv_obj_set_style_text_align(main_label_lux_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_align(main_label_lux_value, LV_ALIGN_CENTER, 0);
    lv_label_set_text_fmt(main_label_lux_value, "%d", current_lux_value);
    lv_obj_set_style_text_font(main_label_lux_value, &lv_font_montserrat_20, LV_STATE_DEFAULT);

    /* 选择cam窗口 */
    cam_win_select = lv_win_create(lv_scr_act(), scr_act_height()/10);        /* 创建窗口 */
    lv_obj_set_size(cam_win_select, 35 * scr_act_width() / 40, 35 * scr_act_height() / 40);
    lv_obj_center(cam_win_select);
    lv_obj_set_style_bg_color(cam_win_select, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(cam_win_select, LV_OPA_80, 0);

    lv_obj_t *cam_header_win = lv_win_get_header(cam_win_select);
    lv_obj_set_style_bg_color(cam_header_win, lv_color_hex(0x2a2a3e), 0);


    lv_obj_set_style_border_width(cam_win_select, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(cam_win_select, lv_color_hex(0x8a8a8a), LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(cam_win_select, LV_OPA_50, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(cam_win_select, 10, LV_STATE_DEFAULT);

    lv_obj_t *cam_title_win = lv_win_add_title(cam_win_select, "Cam select");
    lv_obj_set_style_text_font(cam_title_win, &lv_font_montserrat_14, LV_STATE_DEFAULT);

    lv_obj_t *cam_btn_win_close = lv_win_add_btn(cam_win_select, LV_SYMBOL_CLOSE, 100);
    lv_obj_set_style_bg_opa(cam_btn_win_close, LV_OPA_30, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(cam_btn_win_close, lv_color_hex(0x666666), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(cam_btn_win_close, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(cam_btn_win_close, lv_color_hex(0xff0000), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(cam_btn_win_close, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cam_btn_win_close, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cam_btn_win_close, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(cam_btn_win_close, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(cam_btn_win_close, lv_color_hex(0xffffff), LV_STATE_PRESSED);
    lv_obj_add_event_cb(cam_btn_win_close, cam_close_win_cb, LV_EVENT_CLICKED, NULL);                       /* 添加事件 */
    lv_obj_add_flag(cam_win_select, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *cam_cont_win = lv_win_get_content(cam_win_select);
    lv_obj_set_flex_flow(cam_cont_win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(cam_cont_win, 8, 0);
    lv_obj_set_style_bg_opa(cam_cont_win, LV_OPA_80, 0);

    // 可滚动的卡片区域
    cam_card_win_container = lv_obj_create(cam_cont_win);
    lv_obj_set_size(cam_card_win_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(cam_card_win_container, 1);
    lv_obj_set_flex_flow(cam_card_win_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(cam_card_win_container, 10, 0);
    lv_obj_set_style_pad_column(cam_card_win_container, 0, 0);
    lv_obj_set_style_bg_opa(cam_card_win_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cam_card_win_container, 0, 0);
    lv_obj_set_scrollbar_mode(cam_card_win_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(cam_card_win_container, LV_DIR_VER);


    // 底部固定添加按钮（高度固定，不 grow）
    lv_obj_t *cam_btn_win_add = lv_btn_create(cam_cont_win);
    lv_obj_set_size(cam_btn_win_add, LV_PCT(100), 48);
    lv_obj_set_style_radius(cam_btn_win_add, 8, 0);

    lv_obj_t *cam_label_win_add = lv_label_create(cam_btn_win_add);
    lv_label_set_text(cam_label_win_add, "Add Cam");
    lv_obj_center(cam_label_win_add);

    lv_obj_set_style_bg_color(cam_btn_win_add, lv_color_hex(0x0066cc), 0);
    lv_obj_set_style_text_color(cam_label_win_add, lv_color_white(), 0);
    lv_obj_set_style_bg_color(cam_btn_win_add, lv_color_hex(0x0055aa), LV_STATE_PRESSED);

    // 绑定事件
    lv_obj_add_event_cb(cam_btn_win_add, btn_add_cam_event_cb, LV_EVENT_CLICKED, NULL);

    // 创建键盘对象
    cam_keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_add_flag(cam_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_mode(cam_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    
    // 增大键盘按键尺寸，方便点击
    lv_obj_set_style_width(cam_keyboard, 60, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_height(cam_keyboard, 50, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(cam_keyboard, 8, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(cam_keyboard, &lv_font_montserrat_20, LV_PART_ITEMS | LV_STATE_DEFAULT);
    
    // 为键盘添加完成/取消事件回调
    lv_obj_add_event_cb(cam_keyboard, keyboard_done_cancel_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(cam_keyboard, keyboard_done_cancel_cb, LV_EVENT_CANCEL, NULL);

    // 创建数字键盘（用于焦距、闪光同步、自定义光圈）
    num_keyboard = lv_keyboard_create(lv_scr_act());
    lv_obj_add_flag(num_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_keyboard_set_mode(num_keyboard, LV_KEYBOARD_MODE_NUMBER);
    
    // 增大数字键盘按键尺寸
    lv_obj_set_style_width(num_keyboard, 70, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_height(num_keyboard, 55, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(num_keyboard, 10, LV_PART_ITEMS | LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(num_keyboard, &lv_font_montserrat_22, LV_PART_ITEMS | LV_STATE_DEFAULT);
    
    // 为数字键盘添加完成/取消事件回调
    lv_obj_add_event_cb(num_keyboard, keyboard_done_cancel_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(num_keyboard, keyboard_done_cancel_cb, LV_EVENT_CANCEL, NULL);

    // 初始化主界面
    update_main_ui_from_cam_card();
    update_main_ui_from_len_card();

    /* 选择len窗口 */
    len_win_select = lv_win_create(lv_scr_act(), scr_act_height()/10);
    lv_obj_set_size(len_win_select, 35 * scr_act_width() / 40, 35 * scr_act_height() / 40);
    lv_obj_center(len_win_select);
    lv_obj_set_style_bg_color(len_win_select, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(len_win_select, LV_OPA_80, 0);

    lv_obj_t *len_header_win = lv_win_get_header(len_win_select);
    lv_obj_set_style_bg_color(len_header_win, lv_color_hex(0x2a2a3e), 0);


    lv_obj_set_style_border_width(len_win_select, 1, LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(len_win_select, lv_color_hex(0x8a8a8a), LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(len_win_select, LV_OPA_50, LV_STATE_DEFAULT);
    lv_obj_set_style_radius(len_win_select, 10, LV_STATE_DEFAULT);

    lv_obj_t *len_title_win = lv_win_add_title(len_win_select, "Len select");
    lv_obj_set_style_text_font(len_title_win, &lv_font_montserrat_14, LV_STATE_DEFAULT);

    lv_obj_t *len_btn_win_close = lv_win_add_btn(len_win_select, LV_SYMBOL_CLOSE, 100);
    lv_obj_set_style_bg_opa(len_btn_win_close, LV_OPA_30, LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(len_btn_win_close, lv_color_hex(0x666666), LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(len_btn_win_close, LV_OPA_50, LV_STATE_PRESSED);
    lv_obj_set_style_bg_color(len_btn_win_close, lv_color_hex(0xff0000), LV_STATE_PRESSED);
    lv_obj_set_style_shadow_width(len_btn_win_close, 0, LV_STATE_DEFAULT);
    lv_obj_set_style_pad_all(len_btn_win_close, 15, LV_STATE_DEFAULT);
    lv_obj_set_style_text_font(len_btn_win_close, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(len_btn_win_close, lv_color_hex(0xffffff), LV_STATE_DEFAULT);
    lv_obj_set_style_text_color(len_btn_win_close, lv_color_hex(0xffffff), LV_STATE_PRESSED);
    lv_obj_add_event_cb(len_btn_win_close, len_close_win_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_flag(len_win_select, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t *len_cont_win = lv_win_get_content(len_win_select);
    lv_obj_set_flex_flow(len_cont_win, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(len_cont_win, 8, 0);
    lv_obj_set_style_bg_opa(len_cont_win, LV_OPA_80, 0);

    // 可滚动的卡片区域
    len_card_win_container = lv_obj_create(len_cont_win);
    lv_obj_set_size(len_card_win_container, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(len_card_win_container, 1);
    lv_obj_set_flex_flow(len_card_win_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(len_card_win_container, 10, 0);
    lv_obj_set_style_pad_column(len_card_win_container, 0, 0);
    lv_obj_set_style_bg_opa(len_card_win_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(len_card_win_container, 0, 0);
    lv_obj_set_scrollbar_mode(len_card_win_container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_scroll_dir(len_card_win_container, LV_DIR_VER);


    // 底部固定添加按钮（高度固定，不 grow）
    lv_obj_t *len_btn_win_add = lv_btn_create(len_cont_win);
    lv_obj_set_size(len_btn_win_add, LV_PCT(100), 48);
    lv_obj_set_style_radius(len_btn_win_add, 8, 0);

    lv_obj_t *len_label_win_add = lv_label_create(len_btn_win_add);
    lv_label_set_text(len_label_win_add, "Add Len");
    lv_obj_center(len_label_win_add);

    lv_obj_set_style_bg_color(len_btn_win_add, lv_color_hex(0x0066cc), 0);
    lv_obj_set_style_text_color(len_label_win_add, lv_color_white(), 0);
    lv_obj_set_style_bg_color(len_btn_win_add, lv_color_hex(0x0055aa), LV_STATE_PRESSED);

    // 绑定事件
    lv_obj_add_event_cb(len_btn_win_add, btn_add_len_event_cb, LV_EVENT_CLICKED, NULL);
}

// ──────────────────────────────────────────────
// 获取当前选中的相机和镜头卡片
// ──────────────────────────────────────────────
/**
 * @brief 获取当前选中的相机卡片
 * @return 相机卡片对象指针，如果没有选中则返回NULL
 */
lv_obj_t* app_ui_get_cam_selected_card(void) {
    return cam_selected_card;
}

/**
 * @brief 获取当前选中的镜头卡片
 * @return 镜头卡片对象指针，如果没有选中则返回NULL
 */
lv_obj_t* app_ui_get_len_selected_card(void) {
    return len_selected_card;
}

/**
 * @brief 获取当前选择的拍摄模式
 * @return 拍摄模式枚举值：
 *         - EXPOSURE_MANUAL (0): 手动模式
 *         - EXPOSURE_AUTO (1): 全自动模式
 *         - EXPOSURE_LANDSCAPE (2): 风光模式
 *         - EXPOSURE_PORTRAIT (3): 人像模式
 * @note UI索引直接对应EXPOSURE枚举值，无需转换
 */
uint8_t app_ui_get_selected_mode(void) {
    return selected_idx;
}

// ──────────────────────────────────────────────
// 设置页面 UI
// ──────────────────────────────────────────────
static void ui_setting_page_init(lv_obj_t* parent) {
    lv_obj_set_style_bg_color(parent, lv_color_black(), 0);
    lv_obj_set_style_pad_all(parent, 10, 0);
    
    lv_obj_t* setting_container = lv_obj_create(parent);
    lv_obj_remove_style_all(setting_container);
    lv_obj_set_size(setting_container, lv_pct(100), lv_pct(100));
    lv_obj_set_flex_flow(setting_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(setting_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(setting_container, 8, 0);
    
    lv_obj_t* title_label = lv_label_create(setting_container);
    lv_label_set_text(title_label, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(title_label, lv_color_white(), 0);
    lv_obj_set_style_pad_bottom(title_label, 5, 0);
    
    lv_obj_t* weather_card = lv_obj_create(setting_container);
    lv_obj_set_size(weather_card, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(weather_card, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(weather_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(weather_card, 15, 0);
    lv_obj_set_style_pad_all(weather_card, 15, 0);
    lv_obj_set_style_border_width(weather_card, 0, 0);
    lv_obj_set_flex_flow(weather_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(weather_card, 8, 0);
    
    lv_obj_t* weather_row1 = lv_obj_create(weather_card);
    lv_obj_remove_style_all(weather_row1);
    lv_obj_set_size(weather_row1, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(weather_row1, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weather_row1, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* weather_icon = lv_label_create(weather_row1);
    lv_label_set_text(weather_icon, LV_SYMBOL_IMAGE);
    lv_obj_set_style_text_font(weather_icon, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(weather_icon, lv_color_hex(0xffd700), 0);
    
    lv_obj_t* weather_temp = lv_label_create(weather_row1);
    lv_label_set_text(weather_temp, "26" "\xC2\xB0" "C");
    lv_obj_set_style_text_font(weather_temp, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(weather_temp, lv_color_white(), 0);
    
    lv_obj_t* weather_desc = lv_label_create(weather_row1);
    lv_label_set_text(weather_desc, "Sunny");
    lv_obj_set_style_text_font(weather_desc, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(weather_desc, lv_color_hex(0xcccccc), 0);
    
    lv_obj_t* weather_row2 = lv_obj_create(weather_card);
    lv_obj_remove_style_all(weather_row2);
    lv_obj_set_size(weather_row2, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(weather_row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(weather_row2, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* humidity_label = lv_label_create(weather_row2);
    lv_label_set_text(humidity_label, LV_SYMBOL_TINT " 65%");
    lv_obj_set_style_text_font(humidity_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(humidity_label, lv_color_hex(0x87ceeb), 0);
    
    lv_obj_t* wind_label = lv_label_create(weather_row2);
    lv_label_set_text(wind_label, LV_SYMBOL_REFRESH " 3m/s");
    lv_obj_set_style_text_font(wind_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(wind_label, lv_color_hex(0x90ee90), 0);
    
    lv_obj_t* sunrise_sunset_card = lv_obj_create(setting_container);
    lv_obj_set_size(sunrise_sunset_card, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(sunrise_sunset_card, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(sunrise_sunset_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(sunrise_sunset_card, 15, 0);
    lv_obj_set_style_pad_all(sunrise_sunset_card, 15, 0);
    lv_obj_set_style_border_width(sunrise_sunset_card, 0, 0);
    lv_obj_set_flex_flow(sunrise_sunset_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(sunrise_sunset_card, 5, 0);
    
    lv_obj_t* timeline_title = lv_label_create(sunrise_sunset_card);
    lv_label_set_text(timeline_title, "Sunrise & Sunset Timeline");
    lv_obj_set_style_text_font(timeline_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(timeline_title, lv_color_hex(0xaaaaaa), 0);
    
    lv_obj_t* timeline_bar = lv_obj_create(sunrise_sunset_card);
    lv_obj_remove_style_all(timeline_bar);
    lv_obj_set_size(timeline_bar, lv_pct(100), 20);
    lv_obj_set_style_bg_color(timeline_bar, lv_color_hex(0x333344), 0);
    lv_obj_set_style_bg_opa(timeline_bar, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(timeline_bar, 10, 0);
    
    lv_obj_t* timeline_indicator = lv_obj_create(timeline_bar);
    lv_obj_set_size(timeline_indicator, 12, 12);
    lv_obj_align(timeline_indicator, LV_ALIGN_LEFT_MID, 55, 0);
    lv_obj_set_style_bg_color(timeline_indicator, lv_color_hex(0xffcc00), 0);
    lv_obj_set_style_bg_opa(timeline_indicator, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(timeline_indicator, 6, 0);
    lv_obj_set_style_border_width(timeline_indicator, 0, 0);
    
    lv_obj_t* timeline_labels = lv_obj_create(sunrise_sunset_card);
    lv_obj_remove_style_all(timeline_labels);
    lv_obj_set_size(timeline_labels, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(timeline_labels, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(timeline_labels, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    lv_obj_t* sunrise_label = lv_label_create(timeline_labels);
    lv_label_set_text(sunrise_label, LV_SYMBOL_UP " 06:12");
    lv_obj_set_style_text_font(sunrise_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sunrise_label, lv_color_hex(0xffa500), 0);
    
    lv_obj_t* now_label = lv_label_create(timeline_labels);
    lv_label_set_text(now_label, "Now");
    lv_obj_set_style_text_font(now_label, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(now_label, lv_color_hex(0xffcc00), 0);
    
    lv_obj_t* sunset_label = lv_label_create(timeline_labels);
    lv_label_set_text(sunset_label, LV_SYMBOL_DOWN " 18:35");
    lv_obj_set_style_text_font(sunset_label, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(sunset_label, lv_color_hex(0xff6b6b), 0);
    
    lv_obj_t* cards_row = lv_obj_create(setting_container);
    lv_obj_remove_style_all(cards_row);
    lv_obj_set_size(cards_row, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cards_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cards_row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cards_row, 8, 0);
    
    lv_obj_t* wifi_card = lv_obj_create(cards_row);
    lv_obj_set_size(wifi_card, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(wifi_card, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(wifi_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(wifi_card, 15, 0);
    lv_obj_set_style_pad_all(wifi_card, 12, 0);
    lv_obj_set_style_border_width(wifi_card, 0, 0);
    lv_obj_set_flex_flow(wifi_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(wifi_card, 3, 0);
    
    lv_obj_t* wifi_icon = lv_label_create(wifi_card);
    lv_label_set_text(wifi_icon, LV_SYMBOL_WIFI);
    lv_obj_set_style_text_font(wifi_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(wifi_icon, lv_color_hex(0x00ff00), 0);
    
    lv_obj_t* wifi_title = lv_label_create(wifi_card);
    lv_label_set_text(wifi_title, "WiFi");
    lv_obj_set_style_text_font(wifi_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(wifi_title, lv_color_white(), 0);
    
    lv_obj_t* wifi_status = lv_label_create(wifi_card);
    lv_label_set_text(wifi_status, "MyWiFi");
    lv_obj_set_style_text_font(wifi_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(wifi_status, lv_color_hex(0x888888), 0);
    
    lv_obj_t* time_card = lv_obj_create(cards_row);
    lv_obj_set_size(time_card, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(time_card, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(time_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(time_card, 15, 0);
    lv_obj_set_style_pad_all(time_card, 12, 0);
    lv_obj_set_style_border_width(time_card, 0, 0);
    lv_obj_set_flex_flow(time_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(time_card, 3, 0);
    
    lv_obj_t* time_icon = lv_label_create(time_card);
    lv_label_set_text(time_icon, LV_SYMBOL_BELL);
    lv_obj_set_style_text_font(time_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(time_icon, lv_color_hex(0x87ceeb), 0);
    
    lv_obj_t* time_title = lv_label_create(time_card);
    lv_label_set_text(time_title, "Time");
    lv_obj_set_style_text_font(time_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(time_title, lv_color_white(), 0);
    
    lv_obj_t* time_status = lv_label_create(time_card);
    lv_label_set_text(time_status, "Auto Sync");
    lv_obj_set_style_text_font(time_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(time_status, lv_color_hex(0x888888), 0);
    
    lv_obj_t* cards_row2 = lv_obj_create(setting_container);
    lv_obj_remove_style_all(cards_row2);
    lv_obj_set_size(cards_row2, lv_pct(95), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(cards_row2, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cards_row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(cards_row2, 8, 0);
    
    lv_obj_t* location_card = lv_obj_create(cards_row2);
    lv_obj_set_size(location_card, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(location_card, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(location_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(location_card, 15, 0);
    lv_obj_set_style_pad_all(location_card, 12, 0);
    lv_obj_set_style_border_width(location_card, 0, 0);
    lv_obj_set_flex_flow(location_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(location_card, 3, 0);
    
    lv_obj_t* location_icon = lv_label_create(location_card);
    lv_label_set_text(location_icon, LV_SYMBOL_GPS);
    lv_obj_set_style_text_font(location_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(location_icon, lv_color_hex(0xff6b6b), 0);
    
    lv_obj_t* location_title = lv_label_create(location_card);
    lv_label_set_text(location_title, "Location");
    lv_obj_set_style_text_font(location_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(location_title, lv_color_white(), 0);
    
    lv_obj_t* location_status = lv_label_create(location_card);
    lv_label_set_text(location_status, "Beijing");
    lv_obj_set_style_text_font(location_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(location_status, lv_color_hex(0x888888), 0);
    
    lv_obj_t* about_card = lv_obj_create(cards_row2);
    lv_obj_set_size(about_card, lv_pct(48), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(about_card, lv_color_hex(0x2a2a3e), 0);
    lv_obj_set_style_bg_opa(about_card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(about_card, 15, 0);
    lv_obj_set_style_pad_all(about_card, 12, 0);
    lv_obj_set_style_border_width(about_card, 0, 0);
    lv_obj_set_flex_flow(about_card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(about_card, 3, 0);
    
    lv_obj_t* about_icon = lv_label_create(about_card);
    lv_label_set_text(about_icon, LV_SYMBOL_LIST);
    lv_obj_set_style_text_font(about_icon, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(about_icon, lv_color_hex(0xdda0dd), 0);
    
    lv_obj_t* about_title = lv_label_create(about_card);
    lv_label_set_text(about_title, "About");
    lv_obj_set_style_text_font(about_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(about_title, lv_color_white(), 0);
    
    lv_obj_t* about_status = lv_label_create(about_card);
    lv_label_set_text(about_status, "v1.0.0");
    lv_obj_set_style_text_font(about_status, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(about_status, lv_color_hex(0x888888), 0);
}
