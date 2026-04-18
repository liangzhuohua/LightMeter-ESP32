#ifndef __APP_CONTROLLER_H__
#define __APP_CONTROLLER_H__

#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "hw_wakeup_key.h"

extern QueueHandle_t wifi_operation_queue;
extern QueueHandle_t location_queue;

typedef enum {
    WIFI_OP_NONE = 0,
    WIFI_OP_SCAN,
    WIFI_OP_CONNECT,
    WIFI_OP_DISCONNECT,
    WIFI_OP_ENABLE,
    WIFI_OP_DISABLE
} WifiOperationType;

typedef struct {
    WifiOperationType op;
    char ssid[33];
    char password[65];
} WifiOperationMsg;

void app_controller_init(void);
bool app_controller_request_location(void);
bool app_controller_request_time_sync(void);
bool app_controller_request_weather(void);
void app_controller_enter_deep_sleep(void);
void app_controller_wakeup_key_init(void);

const char* app_controller_get_current_ssid(void);
double app_controller_get_latitude(void);
double app_controller_get_longitude(void);
bool app_controller_get_location_valid(void);

#endif
