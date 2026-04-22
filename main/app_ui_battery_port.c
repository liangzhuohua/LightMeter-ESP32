#include "app_ui_battery_port.h"
#include "lvgl.h"
#include <stdio.h>

extern lv_obj_t *main_table_status;
extern float g_battery_soc;
extern int g_wifi_state;
extern battery_status_t g_battery_status;

static const char *get_wifi_icon(void)
{
    if (g_wifi_state == 2)
        return LV_SYMBOL_WIFI;
    else if (g_wifi_state == 1)
        return LV_SYMBOL_REFRESH;
    else
        return LV_SYMBOL_CLOSE;
}

static void update_status_bar(void)
{
    if (main_table_status == NULL) return;

    const char *wifi_icon = get_wifi_icon();

    const char *batt_icon;
    if (g_battery_soc >= 87.5f)
        batt_icon = LV_SYMBOL_BATTERY_FULL;
    else if (g_battery_soc >= 62.5f)
        batt_icon = LV_SYMBOL_BATTERY_3;
    else if (g_battery_soc >= 37.5f)
        batt_icon = LV_SYMBOL_BATTERY_2;
    else if (g_battery_soc >= 12.5f)
        batt_icon = LV_SYMBOL_BATTERY_1;
    else
        batt_icon = LV_SYMBOL_BATTERY_EMPTY;

    uint8_t soc_int = (uint8_t)g_battery_soc;
    if (soc_int > 100) soc_int = 100;

    if (g_battery_status == BATTERY_STATUS_CHARGING)
        lv_label_set_text_fmt(main_table_status, "%s %d%% #00cc00 %s" LV_SYMBOL_CHARGE "#", wifi_icon, soc_int, batt_icon);
    else if (g_battery_status == BATTERY_STATUS_FULL)
        lv_label_set_text_fmt(main_table_status, "%s %d%% #00cc00 " LV_SYMBOL_BATTERY_FULL "#", wifi_icon, soc_int);
    else
        lv_label_set_text_fmt(main_table_status, "%s %d%% %s", wifi_icon, soc_int, batt_icon);
}

void app_ui_battery_update(float soc_pct, float voltage_mv, battery_status_t status)
{
    g_battery_soc = soc_pct;
    g_battery_status = status;
    update_status_bar();
}
