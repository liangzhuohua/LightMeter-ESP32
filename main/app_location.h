#ifndef __APP_LOCATION_H__
#define __APP_LOCATION_H__

#include "hw_wifi.h"

typedef struct {
    double latitude;
    double longitude;
    float accuracy;
    char address[256];
} location_result_t;

typedef void (*location_result_callback_t)(const location_result_t* result);

void app_location_get_location(wifi_scan_result_t* result, location_result_callback_t callback);

#endif
