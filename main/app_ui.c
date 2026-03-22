#include "app_ui.h"
#include <stdio.h>
#include <math.h>
#include "app_exposure_calc.h"



#define scr_act_width()     lv_obj_get_width(lv_scr_act())              // 宽
#define scr_act_height()    lv_obj_get_height(lv_scr_act())             // 高

static const lv_font_t* font;                                           // 字体

static const char* roller_shutter_options = "1\n1/2\n1/4\n1/8\n1/15\n1/30\n1/60\n1/125\n1/250\n1/500\n1/1000";
static const char* roller_aperture_options = "1.4\n2.0\n2.8\n4.0\n5.6\n8.0\n11.0\n16.0";
static const char* cam_name = "Minolta SR1-S";
static const char* len_name  = "Minolta MD 50mm f/1.4";
static const char* roller_iso_options = "50\n100\n125\n160\n200\n400\n800\n1600\n3200";
static const char* roller_ev_options = "-2\n-1\n-2/3\n-1/3\n0\n+1/3\n+2/3\n+1\n+2";
static const int lux_value = 1000;

// test 

// ================= 辅助函数：快门显示格式化 =================
// 将 float (0.01) 转换为字符串 ("1/100")
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
        // < 1秒，显示分数，例如 1/100
        float denom = 1.0f / shutter;
        // 四舍五入分母
        int denom_int = (int)(denom + 0.5f);
        snprintf(buf, size, "1/%d", denom_int);
    }
}

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

static uint8_t selected_idx = 2;  // 默认选中第3个 (从0开始，手动模式)

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

// ================= 创建模式选择组件 =================
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

        lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, 0);
        lv_obj_set_style_radius(btn, LV_MIN(9, btn_size / 7), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x404040), 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_border_opa(btn, LV_OPA_50, 0);
        lv_obj_set_style_shadow_width(btn, 0, 0);

        if (i == 2) {
            lv_obj_add_state(btn, LV_STATE_CHECKED);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0x4C5C6E), 0);     // 调亮后的青灰
            lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);                 // 强制完全不透明
            lv_obj_set_style_border_width(btn, 2, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0x6A8ABF), 0); // 更亮的边框
        }

        lv_obj_add_event_cb(btn, imgbtn_event_cb, LV_EVENT_CLICKED, (void*)(uintptr_t)i);
    }
}

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

    /* 顶部状态栏布局  flex布局 */
    lv_obj_t* main_flex_layout = lv_obj_create(tile_main);
    lv_obj_remove_style_all(main_flex_layout);
    lv_obj_align(main_flex_layout, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_size(main_flex_layout, lv_pct(96), lv_pct(5));
    lv_obj_set_flex_flow(main_flex_layout, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_flex_layout, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);   // 两端对齐

    /* 左侧状态栏 */
    lv_obj_t* main_table_top_left = lv_label_create(main_flex_layout);
    lv_label_set_text(main_table_top_left, "AM 8:30");
    lv_obj_set_style_text_font(main_table_top_left, font, LV_STATE_DEFAULT);
    
    /* 右侧状态栏 */
    lv_obj_t* main_table_top_right = lv_label_create(main_flex_layout);  
    lv_label_set_text(main_table_top_right, LV_SYMBOL_WIFI "   80% " LV_SYMBOL_BATTERY_3 );
    lv_obj_set_style_text_font(main_table_top_right, font, LV_STATE_DEFAULT);  
/* ----------------------------------------------------------------------------------------------------------------------------------------------------------------- */

/* 主体网格布局 */
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
    
    // for(int row = 0; row < 4; row++) {          // 你有4行
    //     for(int col = 0; col < 2; col++) {      // 你有2列
    //         lv_obj_t * cell = lv_obj_create(main_grid_layout);
    //         lv_obj_remove_style_all(cell);                    // 去掉默认背景和边框
    //         lv_obj_set_style_bg_color(cell, lv_palette_main(LV_PALETTE_GREY), 0);
    //         lv_obj_set_style_bg_opa(cell, LV_OPA_30 + row*20 + col*30, 0);   // 不同格子不同透明度
    //         lv_obj_set_style_border_width(cell, 2, 0);
    //         lv_obj_set_style_border_color(cell, lv_palette_main(LV_PALETTE_BLUE), 0);
    
    //         // 放个数字标签方便看是第几行第几列
    //         lv_obj_t * label = lv_label_create(cell);
    //         lv_label_set_text_fmt(label, "%d,%d", col, row);
    //         lv_obj_center(label);
    
    //         // 设置格子位置
    //         lv_obj_set_grid_cell(cell, LV_GRID_ALIGN_STRETCH, col, 1,
    //                                        LV_GRID_ALIGN_STRETCH, row, 1);
    //     }
    // }    

    /* 相机参数容器 */
    lv_obj_t* main_obj_cam = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_cam, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_radius(main_obj_cam, 15, 0);
    lv_obj_set_flex_flow(main_obj_cam, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj_cam, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);   // 两端对齐

    lv_obj_set_style_pad_all(main_obj_cam, 2, 0);
    lv_obj_set_style_pad_column(main_obj_cam, 4, 0);  // 子对象之间的水平间距
    

    
    // 装图片的容器对象
    lv_obj_t* main_obj_cam_img = lv_obj_create(main_obj_cam);
    lv_obj_set_width(main_obj_cam_img, lv_pct(50));
    lv_obj_set_height(main_obj_cam_img, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_cam_img, 3);   // 宽度按需拉伸
    
    lv_obj_set_style_bg_opa(main_obj_cam_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_cam_img, 0, 0);
    lv_obj_set_scrollbar_mode(main_obj_cam_img, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_cam_img, LV_OBJ_FLAG_SCROLLABLE);


    // 装名字的容器对象
    lv_obj_t* main_obj_cam_name = lv_obj_create(main_obj_cam);
    lv_obj_set_width(main_obj_cam_name, lv_pct(50));
    lv_obj_set_height(main_obj_cam_name, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_cam_name, 4);

    lv_obj_set_style_bg_opa(main_obj_cam_name, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_cam_name, 0, 0);
    lv_obj_set_style_pad_all(main_obj_cam_name, 0, 0);         // 去掉所有内边距
    lv_obj_set_scrollbar_mode(main_obj_cam_name, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_cam_name, LV_OBJ_FLAG_SCROLLABLE);


    // cam图片
    lv_obj_t* main_img_cam = lv_img_create(main_obj_cam_img);
    lv_img_set_src(main_img_cam, &img_cam);
    lv_img_set_zoom(main_img_cam, 140);
    lv_img_set_antialias(main_img_cam, true);
    lv_obj_set_style_img_recolor_opa(main_img_cam, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(main_img_cam, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(main_img_cam);

    // cam_name文字
    lv_obj_t* main_label_cam = lv_label_create(main_obj_cam_name);
    lv_obj_set_style_text_font(main_label_cam, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_cam, lv_pct(100));
    lv_label_set_long_mode(main_label_cam, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(main_label_cam, cam_name);
    lv_obj_center(main_label_cam);


    /* 镜头参数容器 */
    lv_obj_t* main_obj_len = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_len, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    lv_obj_set_style_radius(main_obj_len, 15, 0);
    lv_obj_set_flex_flow(main_obj_len, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(main_obj_len, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);   // 两端对齐

    lv_obj_set_style_pad_all(main_obj_len, 2, 0);
    lv_obj_set_style_pad_column(main_obj_len, 4, 0);  // 子对象之间的水平间距
    
    // 装镜头的容器对象
    lv_obj_t* main_obj_len_img = lv_obj_create(main_obj_len);
    lv_obj_set_width(main_obj_len_img, lv_pct(50));
    lv_obj_set_height(main_obj_len_img, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_len_img, 3);   // 宽度按需拉伸
    
    lv_obj_set_style_bg_opa(main_obj_len_img, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_len_img, 0, 0);
    lv_obj_set_scrollbar_mode(main_obj_len_img, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_len_img, LV_OBJ_FLAG_SCROLLABLE);


    // 装名字的容器对象
    lv_obj_t* main_obj_len_name = lv_obj_create(main_obj_len);
    lv_obj_set_width(main_obj_len_name, lv_pct(50));
    lv_obj_set_height(main_obj_len_name, lv_pct(100));
    lv_obj_set_flex_grow(main_obj_len_name, 4);

    lv_obj_set_style_bg_opa(main_obj_len_name, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(main_obj_len_name, 0, 0);
    lv_obj_set_style_pad_all(main_obj_len_name, 0, 0);         // 去掉所有内边距
    lv_obj_set_scrollbar_mode(main_obj_len_name, LV_SCROLLBAR_MODE_OFF);
    lv_obj_clear_flag(main_obj_len_name, LV_OBJ_FLAG_SCROLLABLE);


    // len图片
    lv_obj_t* main_img_len = lv_img_create(main_obj_len_img);
    lv_img_set_src(main_img_len, &img_len);
    lv_img_set_zoom(main_img_len, 140);
    lv_img_set_antialias(main_img_len, true);
    lv_obj_set_style_img_recolor_opa(main_img_len, LV_OPA_COVER, LV_STATE_DEFAULT);
    lv_obj_set_style_img_recolor(main_img_len, lv_color_white(), LV_STATE_DEFAULT);
    lv_obj_center(main_img_len);

    // len_name文字
    lv_obj_t* main_label_len = lv_label_create(main_obj_len_name);
    lv_obj_set_style_text_font(main_label_len, &lv_font_montserrat_24, LV_STATE_DEFAULT);
    lv_obj_set_width(main_label_len, lv_pct(100));
    lv_label_set_long_mode(main_label_len, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_label_set_text(main_label_len, len_name);
    lv_obj_center(main_label_len);


    /* 快门速度 */
    lv_obj_t* main_obj_shutter = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_shutter, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_radius(main_obj_shutter, 15, 0);
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
    lv_obj_t* main_roller_shutter = lv_roller_create(main_obj_shutter);
    lv_roller_set_options(main_roller_shutter, roller_shutter_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_shutter, true);
    lv_obj_set_width(main_roller_shutter, lv_pct(95));     // 推荐 80%~95%
    lv_obj_set_style_text_align(main_roller_shutter, LV_TEXT_ALIGN_CENTER, 0);  // 文字居中

    /* 镜头光圈 */
    lv_obj_t* main_obj_aperture = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_aperture, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    lv_obj_set_style_radius(main_obj_aperture, 15, 0);
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
    lv_obj_t* main_roller_aperture = lv_roller_create(main_obj_aperture);
    lv_roller_set_options(main_roller_aperture, roller_aperture_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_aperture, true);
    lv_obj_set_width(main_roller_aperture, lv_pct(95));     // 推荐 80%~95%
    lv_obj_set_style_text_align(main_roller_aperture, LV_TEXT_ALIGN_CENTER, 0);  // 文字居中

    /* ISO选择 */
    lv_obj_t* main_obj_iso = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_iso, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
    lv_obj_set_style_radius(main_obj_iso, 15, 0);
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
    lv_obj_t* main_roller_iso = lv_roller_create(main_obj_iso);
    lv_roller_set_options(main_roller_iso, roller_iso_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_iso, false);
    lv_obj_set_width(main_roller_iso, lv_pct(55));          // 调整到合适比例，50~65%
    lv_obj_set_height(main_roller_iso, lv_pct(100));        // 保持占满
    lv_obj_set_style_text_align(main_roller_iso, LV_TEXT_ALIGN_CENTER, 0);
    
    /* EV */
    lv_obj_t* main_obj_ev = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_ev, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 2, 1);
    lv_obj_set_style_radius(main_obj_ev, 15, 0);
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
    lv_obj_t* main_roller_ev = lv_roller_create(main_obj_ev);
    lv_roller_set_options(main_roller_ev, roller_ev_options, LV_ROLLER_MODE_NORMAL);
    style_roller_clean_style(main_roller_ev, false);
    lv_obj_set_width(main_roller_ev, lv_pct(55));          // 调整到合适比例，50~65%
    lv_obj_set_height(main_roller_ev, lv_pct(100));        // 保持占满
    lv_obj_set_style_text_align(main_roller_ev, LV_TEXT_ALIGN_CENTER, 0);


    // lv_obj_set_style_bg_color(main_label_ev, lv_color_hex(0x1A1A2E), 0);     // 很深的蓝黑
    // lv_obj_set_style_bg_opa(main_label_ev, LV_OPA_40, 0);                    // 40% 透明度（自己调 20~70）
    
    // // Lux 数值（建议比标签浅一点或同色系）
    // lv_obj_set_style_bg_color(main_roller_ev, lv_color_hex(0x16213E), 0);
    // lv_obj_set_style_bg_opa(main_roller_ev, LV_OPA_50, 0);


    /* 模式选择 */
    lv_obj_t* main_obj_mode = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_mode, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_set_style_radius(main_obj_mode, 15, LV_STATE_DEFAULT);
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
    lv_obj_t* main_obj_mode_select = lv_obj_create(main_obj_mode);
    lv_obj_set_width(main_obj_mode_select, lv_pct(70));          // 调整到合适比例，50~65%
    lv_obj_set_height(main_obj_mode_select, lv_pct(100));        // 保持占满
    // lv_obj_remove_style_all(main_obj_mode_select);

    create_mode_selector(main_obj_mode_select);
    
    /* lux 显示*/
    lv_obj_t* main_obj_lux = lv_obj_create(main_grid_layout);
    lv_obj_set_grid_cell(main_obj_lux, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 3, 1);
    lv_obj_set_style_radius(main_obj_lux, 15, LV_STATE_DEFAULT);
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
    lv_obj_t* main_label_lux_value = lv_label_create(main_obj_lux);
    lv_obj_set_width(main_label_lux_value, lv_pct(55));          // 调整到合适比例，50~65%
    // lv_obj_set_height(main_label_lux_value, lv_pct(100));        // 保持占满
    lv_obj_set_style_text_align(main_label_lux_value, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_align(main_label_lux_value, LV_ALIGN_CENTER, 0);
    lv_label_set_text_fmt(main_label_lux_value, "%d", lux_value);
    lv_obj_set_style_text_font(main_label_lux_value, &lv_font_montserrat_20, LV_STATE_DEFAULT);



    // Lux 标签
    // lv_obj_set_style_bg_color(main_label_lux, lv_color_hex(0x1A1A2E), 0);     // 很深的蓝黑
    // lv_obj_set_style_bg_opa(main_label_lux, LV_OPA_40, 0);                    // 40% 透明度（自己调 20~70）

    // // Lux 数值（建议比标签浅一点或同色系）
    // lv_obj_set_style_bg_color(main_label_lux_value, lv_color_hex(0x16213E), 0);
    // lv_obj_set_style_bg_opa(main_label_lux_value, LV_OPA_50, 0);


    lv_obj_t* setting_grid_layout = lv_obj_create(tile_setting);
    lv_obj_remove_style_all(setting_grid_layout);
    

    
}

