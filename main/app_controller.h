#ifndef __APP_CONTROLLER_H__
#define __APP_CONTROLLER_H__

#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

extern QueueHandle_t wifi_operation_queue;

typedef enum {
    WIFI_OP_NONE = 0,       // 无操作
    WIFI_OP_SCAN,            // 扫描WiFi
    WIFI_OP_CONNECT,          // 连接WiFi
    WIFI_OP_DISCONNECT,       // 断开WiFi
    WIFI_OP_ENABLE,          // 启用WiFi
    WIFI_OP_DISABLE          // 禁用WiFi
} WifiOperationType;

typedef struct {
    WifiOperationType op;
    char ssid[33];
    char password[65];
} WifiOperationMsg;

void app_controller_init(void);

#endif
