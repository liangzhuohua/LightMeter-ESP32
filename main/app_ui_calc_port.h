#ifndef __APP_UI_CALC_PORT_H__
#define __APP_UI_CALC_PORT_H__

#include "lvgl.h"
#include "app_exposure_calc.h"

// ──────────────────────────────────────────────
// 数据结构：UI与算法之间的数据传输
// ──────────────────────────────────────────────

typedef enum {
    MANUAL_WHEEL_NONE = 0,   // 无滚轮操作
    MANUAL_WHEEL_TV = 1,     // Tv(快门)滚轮
    MANUAL_WHEEL_AV = 2      // Av(光圈)滚轮
} ManualWheelType;

typedef struct {
    float shutter;          // 快门速度
    float aperture;         // 光圈值
    ExposureFlags flags;     // 曝光状态标志
} ui_calc_data_t;

typedef struct {
    char* cam_name;         // 相机名称
    float* shutter_stops;   // 快门档位数组
    int shutter_count;      // 快门档位数量
    float flash_sync;       // 闪光同步速度
} ui_cam_data_t;

typedef struct {
    char* len_name;         // 镜头名称
    float* aperture_stops;  // 光圈档位数组
    int aperture_count;     // 光圈档位数量
    float focal_length;     // 焦距
} ui_len_data_t;

// ──────────────────────────────────────────────
// 初始化与清理
// ──────────────────────────────────────────────

void ui_calc_port_init(void);
void ui_calc_port_deinit(void);

// ──────────────────────────────────────────────
// 数据转换：UI -> 算法
// ──────────────────────────────────────────────

/**
 * @brief 从UI滚轮获取快门值
 * @param roller 快门滚轮对象
 * @return 快门速度(秒)，失败返回-1.0f
 */
float ui_calc_port_get_shutter_from_roller(lv_obj_t* roller);

/**
 * @brief 从UI滚轮获取光圈值
 * @param roller 光圈滚轮对象
 * @return 光圈值，失败返回-1.0f
 */
float ui_calc_port_get_aperture_from_roller(lv_obj_t* roller);

/**
 * @brief 从UI滚轮获取ISO值
 * @param roller ISO滚轮对象
 * @return ISO值，失败返回-1
 */
int ui_calc_port_get_iso_from_roller(lv_obj_t* roller);

/**
 * @brief 从UI滚轮获取EV值
 * @param roller EV滚轮对象
 * @return EV值，失败返回0.0f
 */
float ui_calc_port_get_ev_from_roller(lv_obj_t* roller);

/**
 * @brief 从UI标签获取Lux值
 * @param label Lux标签对象
 * @return Lux值，失败返回-1
 */
int ui_calc_port_get_lux_from_label(lv_obj_t* label);

// ──────────────────────────────────────────────
// 数据转换：算法 -> UI
// ──────────────────────────────────────────────

/**
 * @brief 将快门值设置到 UI 滚轮
 * @param roller 快门滚轮对象
 * @param shutter 快门速度 (秒)
 * @param cam 相机参数结构体，包含快门档位数组
 */
void ui_calc_port_set_shutter_to_roller(lv_obj_t* roller, float shutter, CAM cam);

/**
 * @brief 将光圈值设置到 UI 滚轮
 * @param roller 光圈滚轮对象
 * @param aperture 光圈值
 * @param len 镜头参数结构体，包含光圈档位数组
 */
void ui_calc_port_set_aperture_to_roller(lv_obj_t* roller, float aperture, LEN len);

/**
 * @brief 更新Lux显示
 * @param label Lux标签对象
 * @param lux Lux值
 */
void ui_calc_port_update_lux_label(lv_obj_t* label, int lux);

// ──────────────────────────────────────────────
// 相机/镜头数据提取
// ──────────────────────────────────────────────

/**
 * @brief 从相机卡片提取CAM结构体
 * @param card 相机卡片对象
 * @return CAM结构体，调用者需要释放shutter_stops内存
 */
CAM ui_calc_port_extract_cam_from_card(lv_obj_t* card);

/**
 * @brief 从镜头卡片提取LEN结构体
 * @param card 镜头卡片对象
 * @return LEN结构体，调用者需要释放aperture_stops内存
 */
LEN ui_calc_port_extract_len_from_card(lv_obj_t* card);

// ──────────────────────────────────────────────
// 动态生成选项字符串
// ──────────────────────────────────────────────

/**
 * @brief 根据当前选中的相机卡片动态生成快门选项字符串
 * @return 快门选项字符串（使用\n分隔），如果没有选中卡片则返回NULL
 * @note 此函数会从cam_selected_card中提取快门数组并生成选项字符串
 */
char* ui_calc_port_get_shutter_options(void);

/**
 * @brief 根据当前选中的镜头卡片动态生成光圈选项字符串
 * @return 光圈选项字符串（使用\n分隔），如果没有选中卡片则返回NULL
 * @note 此函数会从len_selected_card中提取光圈数组并生成选项字符串
 */
char* ui_calc_port_get_aperture_options(void);


/**
 * @brief 获取手动模式滚轮标志位
 * @return 当前滚轮操作类型：MANUAL_WHEEL_NONE/MANUAL_WHEEL_TV/MANUAL_WHEEL_AV
 * @note 此标志位只有在mode为手动模式(EXPOSURE_MANUAL)时才有意义
 */
ManualWheelType ui_calc_port_get_manual_wheel_type(void);

/**
 * @brief 获取当前曝光模式
 * @return 当前曝光模式：EXPOSURE_MANUAL/EXPOSURE_AUTO/EXPOSURE_LANDSCAPE/EXPOSURE_PORTRAIT
 * @note 从UI层获取当前选择的拍摄模式
 */
uint8_t ui_calc_port_get_exposure_mode(void);

/**
 * @brief 执行曝光计算（整合UI和算法）
 * @param lux 环境照度
 * @param iso ISO值
 * @param ev 曝光补偿
 * @param mode 拍摄模式
 * @param cam 相机参数
 * @param len 镜头参数
 * @param roller_shutter 快门滚轮（用于读取值）
 * @param roller_aperture 光圈滚轮（用于读取值）
 * @return 计算结果数据
 * @note 根据模式自动选择计算方式：
 *       - 自动模式：调用exposure_auto计算光圈和快门
 *       - 手动模式：根据滚轮类型调用优先计算
 *         * 滑动Tv滚轮：使用快门优先计算光圈
 *         * 滑动Av滚轮：使用光圈优先计算快门
 */
ui_calc_data_t ui_calc_port_exposure(uint32_t lux, int iso, float ev, uint8_t mode,
                                        CAM cam, LEN len,
                                        lv_obj_t* roller_shutter, lv_obj_t* roller_aperture);

/**
 * @brief 设置手动模式滚轮标志位
 * @param type 滚轮操作类型：MANUAL_WHEEL_NONE/MANUAL_WHEEL_TV/MANUAL_WHEEL_AV
 * @note 由UI层调用，当用户滑动Tv或Av滚轮时设置此标志位
 */
void ui_calc_port_set_manual_wheel_type(ManualWheelType type);

/**
 * @brief 重置手动模式滚轮标志位
 * @note 在每次曝光计算完成后调用，以清除之前的滚轮操作记录
 */
void ui_calc_port_reset_manual_wheel_type(void);

// ──────────────────────────────────────────────
// 警告颜色显示
// ──────────────────────────────────────────────

/**
 * @brief 根据曝光标志位更新 Tv/Av roller 选中项的颜色
 * @param shutter_roller 快门滚轮对象
 * @param aperture_roller 光圈滚轮对象
 * @param flags 曝光计算返回的标志位
 *
 * 颜色规则（优先级从高到低）：
 *   - overexposure: 橙色 #FF8800 (Tv+Av)
 *   - underexposure: 蓝色 #4488FF (Tv+Av)
 *   - shutter_out_of_range: 红色 #FF4444 (仅Tv)
 *   - aperture_out_of_range: 红色 #FF4444 (仅Av)
 *   - slow_shutter_warning: 黄色 #FFCC00 (仅Tv)
 *   - 正常: 白色 #FFFFFF
 */
void ui_calc_port_update_roller_warning_color(lv_obj_t* shutter_roller, lv_obj_t* aperture_roller, ExposureFlags flags);

// ──────────────────────────────────────────────
// 配置保存
// ──────────────────────────────────────────────

/**
 * @brief 保存配置到NVS
 * @note 保存相机、镜头等配置数据
 */
void ui_calc_port_save_config(void);

#endif // __APP_UI_CALC_PORT_H__
