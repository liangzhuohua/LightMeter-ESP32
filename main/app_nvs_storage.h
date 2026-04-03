#ifndef __APP_NVS_STORAGE_H__
#define __APP_NVS_STORAGE_H__

#include <stdbool.h>
#include <stdint.h>
#include "app_weather.h"

int app_nvs_storage_init(void);

int app_nvs_save_all(void);
int app_nvs_load_all(void);

int app_nvs_save_cameras(void);
int app_nvs_load_cameras(void);

int app_nvs_save_lens(void);
int app_nvs_load_lens(void);

int app_nvs_save_ui_state(void);
int app_nvs_load_ui_state(void);

int app_nvs_save_wifi(void);
int app_nvs_load_wifi(void);

int app_nvs_save_location(void);
int app_nvs_load_location(void);

typedef struct {
    char name[64];
    int step_type;
    int min_shutter_idx;
    int max_shutter_idx;
    char flash_sync[16];
} camera_data_t;

typedef struct {
    char name[64];
    int step_type;
    int min_aperture_idx;
    int max_aperture_idx;
    char focal_length[16];
    float custom_aperture[32];
    int custom_aperture_count;
} lens_data_t;

typedef struct {
    int iso_idx;
    int ev_idx;
    int mode_idx;
    int shutter_idx;
    int aperture_idx;
    int selected_camera_idx;
    int selected_lens_idx;
} ui_state_data_t;

typedef struct {
    char ssid[33];
    char password[65];
    bool enabled;
} wifi_data_t;

typedef struct {
    double latitude;
    double longitude;
    bool valid;
    char city[64];
    char detail[128];
} location_data_t;

int app_nvs_save_weather(void);
int app_nvs_load_weather(void);

int app_nvs_get_wifi_config(wifi_data_t* wifi);
int app_nvs_get_location_data(location_data_t* location);
int app_nvs_get_weather_data(weather_data_t* weather);

void app_nvs_set_wifi_enabled(bool enabled);
void app_nvs_set_location(double latitude, double longitude, bool valid);
void app_nvs_set_location_text(const char* city, const char* detail);
void app_nvs_set_weather(const weather_data_t* weather);

typedef struct {
    int64_t last_time_sync;
    int64_t last_weather_sync;
    int64_t last_location_sync;
} sync_timestamp_t;

int app_nvs_save_sync_timestamps(const sync_timestamp_t* ts);
int app_nvs_load_sync_timestamps(sync_timestamp_t* ts);
void app_nvs_update_time_sync_timestamp(void);
void app_nvs_update_weather_sync_timestamp(void);
void app_nvs_update_location_sync_timestamp(void);

#endif
