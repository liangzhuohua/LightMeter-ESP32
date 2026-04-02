#ifndef __HW_WIFI_H__
#define __HW_WIFI_H__

#include <stdint.h>

typedef struct {
    char ssid[33];      // WiFi名称
    int8_t rssi;        // 信号强度（dBm）
    uint8_t channel;    // 信道
    uint8_t authmode;   // 认证模式
    uint8_t bssid[6];   // MAC地址
} wifi_info_t;

typedef struct {
    wifi_info_t* ap_list;   // WiFi列表
    uint16_t count;          // WiFi数量
} wifi_scan_result_t;

typedef void (*wifi_scan_done_cb_t)(wifi_scan_result_t* result);
typedef enum {
    HW_WIFI_STATE_CONNECTING = 0,
    HW_WIFI_STATE_CONNECTED,
    HW_WIFI_STATE_DISCONNECTED,
    HW_WIFI_STATE_CONNECT_FAILED
} hw_wifi_state_t;

typedef struct {
    hw_wifi_state_t state;
    char ssid[33];
    int reason;
} hw_wifi_state_event_t;

typedef void (*wifi_state_cb_t)(const hw_wifi_state_event_t *event);

void hw_wifi_init(void);
void hw_wifi_init_with_scan_cb(wifi_scan_done_cb_t callback);
void hw_wifi_register_state_cb(wifi_state_cb_t callback);
wifi_scan_result_t hw_wifi_scan(void);
void hw_wifi_scan_async(void);
void hw_wifi_deinit(void);
void hw_wifi_connect(char* SSID, char* PASSWORD);
void hw_wifi_disconnect(void);
void hw_wifi_scan_result_free(wifi_scan_result_t* result);
const char* hw_wifi_get_current_ssid(void);
const char* hw_wifi_get_current_password(void);

#endif
