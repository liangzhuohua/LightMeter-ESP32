#include "app_nvs_storage.h"
#include "hw_nvs.h"
#include "hw_wifi.h"
#include "app_ui.h"
#include "app_controller.h"
#include "hw_oled.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>
#include "lvgl.h"

static const char* TAG = "app_nvs_storage";

static const char* NS_CAMERA = "camera";
static const char* NS_LENS = "lens";
static const char* NS_UI_STATE = "ui_state";
static const char* NS_WIFI = "wifi";
static const char* NS_LOCATION = "location";
static const char* NS_WEATHER = "weather";

static location_data_t g_cached_location = {0};
static weather_data_t g_cached_weather = {0};
static bool g_weather_cached = false;
static wifi_data_t g_cached_wifi = {0};

typedef struct {
    lv_obj_t *dropdown_shutter_step;
    lv_obj_t *dropdown_min_shutter;
    lv_obj_t *dropdown_max_shutter;
    lv_obj_t *textarea_flash_sync;
    int current_step_type;
} cam_card_params_t;

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

int app_nvs_storage_init(void) {
    ESP_LOGI(TAG, "NVS storage module initialized");
    return 0;
}

int app_nvs_save_cameras(void) {
    ESP_LOGI(TAG, "Saving cameras to NVS...");

    lv_obj_t* container = app_ui_get_cam_container();
    if (container == NULL) {
        ESP_LOGW(TAG, "Camera container is NULL");
        return 0;
    }

    uint32_t count = lv_obj_get_child_cnt(container);
    hw_nvs_set_int(NS_CAMERA, "count", (int32_t)count);

    int selected_idx = app_ui_get_cam_selected_index();
    hw_nvs_set_int(NS_CAMERA, "sel_idx", (int32_t)selected_idx);

    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* card = lv_obj_get_child(container, i);
        if (card == NULL) continue;

        cam_card_params_t* params = (cam_card_params_t*)lv_obj_get_user_data(card);
        if (params == NULL) continue;

        char key[16];

        snprintf(key, sizeof(key), "c%d_st", i);
        hw_nvs_set_int(NS_CAMERA, key, params->current_step_type);

        if (params->dropdown_min_shutter != NULL) {
            snprintf(key, sizeof(key), "c%d_min", i);
            hw_nvs_set_int(NS_CAMERA, key, (int32_t)lv_dropdown_get_selected(params->dropdown_min_shutter));
        }

        if (params->dropdown_max_shutter != NULL) {
            snprintf(key, sizeof(key), "c%d_max", i);
            hw_nvs_set_int(NS_CAMERA, key, (int32_t)lv_dropdown_get_selected(params->dropdown_max_shutter));
        }

        if (params->textarea_flash_sync != NULL) {
            snprintf(key, sizeof(key), "c%d_fs", i);
            const char* flash_sync = lv_textarea_get_text(params->textarea_flash_sync);
            if (flash_sync != NULL && strlen(flash_sync) > 0) {
                hw_nvs_set_string(NS_CAMERA, key, flash_sync);
            }
        }

        lv_obj_t* card_content = lv_obj_get_child(card, 0);
        if (card_content) {
            lv_obj_t* row1 = lv_obj_get_child(card_content, 0);
            if (row1) {
                lv_obj_t* name_ta = lv_obj_get_child(row1, 0);
                if (name_ta) {
                    const char* name_text = lv_textarea_get_text(name_ta);
                    if (name_text != NULL && strlen(name_text) > 0) {
                        snprintf(key, sizeof(key), "c%d_nm", i);
                        hw_nvs_set_string(NS_CAMERA, key, name_text);
                    }
                }
            }
        }
    }

    ESP_LOGI(TAG, "Saved %d cameras", count);
    return 0;
}

int app_nvs_load_cameras(void) {
    ESP_LOGI(TAG, "Loading cameras from NVS...");

    int32_t count = 0;
    if (hw_nvs_get_int(NS_CAMERA, "count", &count) != 0 || count == 0) {
        ESP_LOGI(TAG, "No cameras to load");
        return 0;
    }

    ESP_LOGI(TAG, "Found %d cameras in NVS, restoring...", count);

    if (!example_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to get LVGL lock for camera restore");
        return -1;
    }

    for (int32_t i = 0; i < count; i++) {
        char key[16];
        int32_t step_type = 0, min_idx = 0, max_idx = 0;
        char name[64] = {0};
        char flash_sync[32] = {0};
        size_t len;

        snprintf(key, sizeof(key), "c%d_st", i);
        hw_nvs_get_int(NS_CAMERA, key, &step_type);

        snprintf(key, sizeof(key), "c%d_min", i);
        hw_nvs_get_int(NS_CAMERA, key, &min_idx);

        snprintf(key, sizeof(key), "c%d_max", i);
        hw_nvs_get_int(NS_CAMERA, key, &max_idx);

        snprintf(key, sizeof(key), "c%d_nm", i);
        len = sizeof(name);
        hw_nvs_get_string(NS_CAMERA, key, name, &len);

        snprintf(key, sizeof(key), "c%d_fs", i);
        len = sizeof(flash_sync);
        hw_nvs_get_string(NS_CAMERA, key, flash_sync, &len);

        ESP_LOGI(TAG, "Restoring camera %d: name=%s, step=%d, min=%d, max=%d, flash=%s",
                 i, name, step_type, min_idx, max_idx, flash_sync);

        app_ui_create_cam_card(name, step_type, min_idx, max_idx, flash_sync);
    }

    int32_t selected_idx = 0;
    hw_nvs_get_int(NS_CAMERA, "sel_idx", &selected_idx);
    if (selected_idx >= 0) {
        app_ui_set_cam_selected_index((int)selected_idx);
        ESP_LOGI(TAG, "Selected camera index restored: %d", selected_idx);
    }

    example_lvgl_unlock();

    return 0;
}

int app_nvs_save_lens(void) {
    ESP_LOGI(TAG, "Saving lens to NVS...");

    lv_obj_t* container = app_ui_get_len_container();
    if (container == NULL) {
        ESP_LOGW(TAG, "Lens container is NULL");
        return 0;
    }

    uint32_t count = lv_obj_get_child_cnt(container);
    hw_nvs_set_int(NS_LENS, "count", (int32_t)count);

    int selected_idx = app_ui_get_len_selected_index();
    hw_nvs_set_int(NS_LENS, "sel_idx", (int32_t)selected_idx);

    for (uint32_t i = 0; i < count; i++) {
        lv_obj_t* card = lv_obj_get_child(container, i);
        if (card == NULL) continue;

        len_card_params_t* params = (len_card_params_t*)lv_obj_get_user_data(card);
        if (params == NULL) continue;

        char key[16];

        snprintf(key, sizeof(key), "l%d_st", i);
        hw_nvs_set_int(NS_LENS, key, params->current_step_type);

        if (params->dropdown_min_aperture != NULL) {
            snprintf(key, sizeof(key), "l%d_min", i);
            hw_nvs_set_int(NS_LENS, key, (int32_t)lv_dropdown_get_selected(params->dropdown_min_aperture));
        }

        if (params->dropdown_max_aperture != NULL) {
            snprintf(key, sizeof(key), "l%d_max", i);
            hw_nvs_set_int(NS_LENS, key, (int32_t)lv_dropdown_get_selected(params->dropdown_max_aperture));
        }

        if (params->textarea_focal_length != NULL) {
            snprintf(key, sizeof(key), "l%d_fl", i);
            const char* focal_length = lv_textarea_get_text(params->textarea_focal_length);
            if (focal_length != NULL && strlen(focal_length) > 0) {
                hw_nvs_set_string(NS_LENS, key, focal_length);
            }
        }

        snprintf(key, sizeof(key), "l%d_cc", i);
        hw_nvs_set_int(NS_LENS, key, params->custom_aperture_count);

        // 暂时跳过保存 custom_aperture_array，可能有内存问题
        // if (params->custom_aperture_count > 0 && params->custom_aperture_array != NULL) {
        //     snprintf(key, sizeof(key), "l%d_ca", i);
        //     hw_nvs_set_blob(NS_LENS, key, params->custom_aperture_array,
        //                    params->custom_aperture_count * sizeof(float));
        // }

        lv_obj_t* card_content = lv_obj_get_child(card, 0);
        if (card_content) {
            lv_obj_t* row1 = lv_obj_get_child(card_content, 0);
            if (row1) {
                lv_obj_t* name_ta = lv_obj_get_child(row1, 0);
                if (name_ta) {
                    const char* name_text = lv_textarea_get_text(name_ta);
                    if (name_text != NULL && strlen(name_text) > 0) {
                        snprintf(key, sizeof(key), "l%d_nm", i);
                        hw_nvs_set_string(NS_LENS, key, name_text);
                    }
                }
            }
        }
    }

    ESP_LOGI(TAG, "Saved %d lens", count);
    return 0;
}

int app_nvs_load_lens(void) {
    ESP_LOGI(TAG, "Loading lens from NVS...");

    int32_t count = 0;
    if (hw_nvs_get_int(NS_LENS, "count", &count) != 0 || count == 0) {
        ESP_LOGI(TAG, "No lens to load");
        return 0;
    }

    ESP_LOGI(TAG, "Found %d lens in NVS, restoring...", count);

    if (!example_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to get LVGL lock for lens restore");
        return -1;
    }

    for (int32_t i = 0; i < count; i++) {
        char key[16];
        int32_t step_type = 0, min_idx = 0, max_idx = 0;
        char name[64] = {0};
        char focal_length[32] = {0};
        size_t len;

        snprintf(key, sizeof(key), "l%d_st", i);
        hw_nvs_get_int(NS_LENS, key, &step_type);

        snprintf(key, sizeof(key), "l%d_min", i);
        hw_nvs_get_int(NS_LENS, key, &min_idx);

        snprintf(key, sizeof(key), "l%d_max", i);
        hw_nvs_get_int(NS_LENS, key, &max_idx);

        snprintf(key, sizeof(key), "l%d_nm", i);
        len = sizeof(name);
        hw_nvs_get_string(NS_LENS, key, name, &len);

        snprintf(key, sizeof(key), "l%d_fl", i);
        len = sizeof(focal_length);
        hw_nvs_get_string(NS_LENS, key, focal_length, &len);

        ESP_LOGI(TAG, "Restoring lens %d: name=%s, step=%d, min=%d, max=%d, focal=%s",
                 i, name, step_type, min_idx, max_idx, focal_length);

        app_ui_create_len_card(name, step_type, min_idx, max_idx, focal_length);
    }

    int32_t selected_idx = 0;
    hw_nvs_get_int(NS_LENS, "sel_idx", &selected_idx);
    if (selected_idx >= 0) {
        app_ui_set_len_selected_index((int)selected_idx);
        ESP_LOGI(TAG, "Selected lens index restored: %d", selected_idx);
    }

    example_lvgl_unlock();

    return 0;
}

int app_nvs_save_ui_state(void) {
    ESP_LOGI(TAG, "Saving UI state to NVS...");

    extern lv_obj_t* main_roller_iso;
    extern lv_obj_t* main_roller_ev;
    extern lv_obj_t* main_roller_shutter;
    extern lv_obj_t* main_roller_aperture;

    hw_nvs_set_int(NS_UI_STATE, "iso_idx", (int32_t)app_ui_get_roller_selected(main_roller_iso));
    hw_nvs_set_int(NS_UI_STATE, "ev_idx", (int32_t)app_ui_get_roller_selected(main_roller_ev));
    hw_nvs_set_int(NS_UI_STATE, "mode_idx", (int32_t)app_ui_get_selected_mode());
    hw_nvs_set_int(NS_UI_STATE, "shutter_idx", (int32_t)app_ui_get_roller_selected(main_roller_shutter));
    hw_nvs_set_int(NS_UI_STATE, "aperture_idx", (int32_t)app_ui_get_roller_selected(main_roller_aperture));
    hw_nvs_set_int(NS_UI_STATE, "sel_cam_idx", (int32_t)app_ui_get_cam_selected_index());
    hw_nvs_set_int(NS_UI_STATE, "sel_len_idx", (int32_t)app_ui_get_len_selected_index());

    ESP_LOGI(TAG, "UI state saved");
    return 0;
}

int app_nvs_load_ui_state(void) {
    ESP_LOGI(TAG, "Loading UI state from NVS...");

    extern lv_obj_t* main_roller_iso;
    extern lv_obj_t* main_roller_ev;
    extern lv_obj_t* main_roller_shutter;
    extern lv_obj_t* main_roller_aperture;

    int32_t value;

    if (!example_lvgl_lock(-1)) {
        ESP_LOGE(TAG, "Failed to get LVGL lock for UI state restore");
        return -1;
    }

    ESP_LOGI(TAG, "main_roller_iso=%p, main_roller_ev=%p",
             (void*)main_roller_iso, (void*)main_roller_ev);

    if (hw_nvs_get_int(NS_UI_STATE, "iso_idx", &value) == 0 && main_roller_iso) {
        uint16_t option_cnt = lv_roller_get_option_cnt(main_roller_iso);
        ESP_LOGI(TAG, "ISO: saved_idx=%d, option_cnt=%d", value, option_cnt);
        if (value >= 0 && value < option_cnt) {
            lv_roller_set_selected(main_roller_iso, (uint16_t)value, LV_ANIM_OFF);
            ESP_LOGI(TAG, "ISO index restored: %d", value);
        }
    }
    if (hw_nvs_get_int(NS_UI_STATE, "ev_idx", &value) == 0 && main_roller_ev) {
        uint16_t option_cnt = lv_roller_get_option_cnt(main_roller_ev);
        ESP_LOGI(TAG, "EV: saved_idx=%d, option_cnt=%d", value, option_cnt);
        if (value >= 0 && value < option_cnt) {
            lv_roller_set_selected(main_roller_ev, (uint16_t)value, LV_ANIM_OFF);
            ESP_LOGI(TAG, "EV index restored: %d", value);
        }
    }
    if (hw_nvs_get_int(NS_UI_STATE, "mode_idx", &value) == 0 && value >= 0 && value < 4) {
        app_ui_set_selected_mode((uint8_t)value);
        ESP_LOGI(TAG, "Mode index restored: %d", value);
    }

    example_lvgl_unlock();

    ESP_LOGI(TAG, "UI state loaded");
    return 0;
}

int app_nvs_save_wifi(void) {
    ESP_LOGI(TAG, "Saving WiFi config to NVS...");

    const char* ssid = app_controller_get_current_ssid();
    const char* password = hw_wifi_get_current_password();

    hw_nvs_set_string(NS_WIFI, "ssid", ssid ? ssid : "");
    hw_nvs_set_string(NS_WIFI, "pwd", password ? password : "");
    hw_nvs_set_bool(NS_WIFI, "enabled", g_cached_wifi.enabled);

    ESP_LOGI(TAG, "WiFi config saved: %s, enabled=%d", ssid ? ssid : "NULL", g_cached_wifi.enabled);
    return 0;
}

int app_nvs_load_wifi(void) {
    ESP_LOGI(TAG, "Loading WiFi config from NVS...");

    char ssid[33] = {0};
    size_t len = sizeof(ssid);

    if (hw_nvs_get_string(NS_WIFI, "ssid", ssid, &len) == 0) {
        ESP_LOGI(TAG, "WiFi SSID: %s", ssid);
        return 0;
    }

    return -1;
}

int app_nvs_get_wifi_config(wifi_data_t* wifi) {
    if (wifi == NULL) return -1;

    memset(wifi, 0, sizeof(wifi_data_t));

    size_t ssid_len = sizeof(wifi->ssid);
    if (hw_nvs_get_string(NS_WIFI, "ssid", wifi->ssid, &ssid_len) != 0) {
        return -1;
    }

    size_t pwd_len = sizeof(wifi->password);
    hw_nvs_get_string(NS_WIFI, "pwd", wifi->password, &pwd_len);

    bool enabled = false;
    if (hw_nvs_get_bool(NS_WIFI, "enabled", &enabled) == 0) {
        wifi->enabled = enabled;
        g_cached_wifi.enabled = enabled;
    } else {
        wifi->enabled = false;
        g_cached_wifi.enabled = false;
    }

    strncpy(g_cached_wifi.ssid, wifi->ssid, sizeof(g_cached_wifi.ssid) - 1);
    strncpy(g_cached_wifi.password, wifi->password, sizeof(g_cached_wifi.password) - 1);

    ESP_LOGI(TAG, "Loaded WiFi config: SSID=%s, enabled=%d", wifi->ssid, wifi->enabled);
    return 0;
}

void app_nvs_set_wifi_enabled(bool enabled) {
    g_cached_wifi.enabled = enabled;
    hw_nvs_set_bool(NS_WIFI, "enabled", enabled);
    ESP_LOGI(TAG, "WiFi enabled state set to: %d", enabled);
}

int app_nvs_save_location(void) {
    ESP_LOGI(TAG, "Saving location to NVS...");

    double latitude = app_controller_get_latitude();
    double longitude = app_controller_get_longitude();
    bool valid = app_controller_get_location_valid();

    char buf[32];

    snprintf(buf, sizeof(buf), "%.6f", latitude);
    hw_nvs_set_string(NS_LOCATION, "latitude", buf);

    snprintf(buf, sizeof(buf), "%.6f", longitude);
    hw_nvs_set_string(NS_LOCATION, "longitude", buf);

    hw_nvs_set_bool(NS_LOCATION, "valid", valid);

    hw_nvs_set_string(NS_LOCATION, "city", g_cached_location.city);
    hw_nvs_set_string(NS_LOCATION, "detail", g_cached_location.detail);

    g_cached_location.latitude = latitude;
    g_cached_location.longitude = longitude;
    g_cached_location.valid = valid;

    ESP_LOGI(TAG, "Location saved: %.6f, %.6f, city=%s", latitude, longitude, g_cached_location.city);
    return 0;
}

int app_nvs_load_location(void) {
    ESP_LOGI(TAG, "Loading location from NVS...");

    char buf[32];
    size_t len = sizeof(buf);

    if (hw_nvs_get_string(NS_LOCATION, "latitude", buf, &len) == 0) {
        g_cached_location.latitude = atof(buf);
        ESP_LOGI(TAG, "Latitude: %s", buf);
    }

    len = sizeof(buf);
    if (hw_nvs_get_string(NS_LOCATION, "longitude", buf, &len) == 0) {
        g_cached_location.longitude = atof(buf);
        ESP_LOGI(TAG, "Longitude: %s", buf);
    }

    bool valid = false;
    if (hw_nvs_get_bool(NS_LOCATION, "valid", &valid) == 0) {
        g_cached_location.valid = valid;
        ESP_LOGI(TAG, "Valid: %d", valid);
    }

    len = sizeof(g_cached_location.city);
    if (hw_nvs_get_string(NS_LOCATION, "city", g_cached_location.city, &len) == 0) {
        ESP_LOGI(TAG, "City: %s", g_cached_location.city);
    }

    len = sizeof(g_cached_location.detail);
    if (hw_nvs_get_string(NS_LOCATION, "detail", g_cached_location.detail, &len) == 0) {
        ESP_LOGI(TAG, "Detail: %s", g_cached_location.detail);
    }

    return 0;
}

int app_nvs_get_location_data(location_data_t* location) {
    if (location == NULL) return -1;
    *location = g_cached_location;
    return 0;
}

void app_nvs_set_location(double latitude, double longitude, bool valid) {
    g_cached_location.latitude = latitude;
    g_cached_location.longitude = longitude;
    g_cached_location.valid = valid;
}

void app_nvs_set_location_text(const char* city, const char* detail) {
    if (city) {
        strncpy(g_cached_location.city, city, sizeof(g_cached_location.city) - 1);
        g_cached_location.city[sizeof(g_cached_location.city) - 1] = '\0';
    }
    if (detail) {
        strncpy(g_cached_location.detail, detail, sizeof(g_cached_location.detail) - 1);
        g_cached_location.detail[sizeof(g_cached_location.detail) - 1] = '\0';
    }
}

int app_nvs_save_weather(void) {
    ESP_LOGI(TAG, "Saving weather to NVS...");

    hw_nvs_set_int(NS_WEATHER, "temp", g_cached_weather.temp);
    hw_nvs_set_int(NS_WEATHER, "temp_max", g_cached_weather.temp_max);
    hw_nvs_set_int(NS_WEATHER, "temp_min", g_cached_weather.temp_min);
    hw_nvs_set_string(NS_WEATHER, "desc", g_cached_weather.desc);
    hw_nvs_set_string(NS_WEATHER, "icon", g_cached_weather.icon);
    hw_nvs_set_int(NS_WEATHER, "humidity", g_cached_weather.humidity);

    char buf[16];
    snprintf(buf, sizeof(buf), "%.1f", g_cached_weather.wind_speed);
    hw_nvs_set_string(NS_WEATHER, "wind", buf);

    hw_nvs_set_int(NS_WEATHER, "sr_h", g_cached_weather.sunrise_hour);
    hw_nvs_set_int(NS_WEATHER, "sr_m", g_cached_weather.sunrise_minute);
    hw_nvs_set_int(NS_WEATHER, "ss_h", g_cached_weather.sunset_hour);
    hw_nvs_set_int(NS_WEATHER, "ss_m", g_cached_weather.sunset_minute);
    hw_nvs_set_int(NS_WEATHER, "mr_h", g_cached_weather.moonrise_hour);
    hw_nvs_set_int(NS_WEATHER, "mr_m", g_cached_weather.moonrise_minute);
    hw_nvs_set_int(NS_WEATHER, "ms_h", g_cached_weather.moonset_hour);
    hw_nvs_set_int(NS_WEATHER, "ms_m", g_cached_weather.moonset_minute);
    hw_nvs_set_string(NS_WEATHER, "moon", g_cached_weather.moon_phase);
    hw_nvs_set_string(NS_WEATHER, "moon_i", g_cached_weather.moon_phase_icon);

    ESP_LOGI(TAG, "Weather saved: %d°C, %s", g_cached_weather.temp, g_cached_weather.desc);
    return 0;
}

int app_nvs_load_weather(void) {
    ESP_LOGI(TAG, "Loading weather from NVS...");

    int32_t value;
    char buf[32];
    size_t len;

    if (hw_nvs_get_int(NS_WEATHER, "temp", &value) == 0) {
        g_cached_weather.temp = (int)value;
    } else {
        return -1;
    }

    hw_nvs_get_int(NS_WEATHER, "temp_max", &value);
    g_cached_weather.temp_max = (int)value;

    hw_nvs_get_int(NS_WEATHER, "temp_min", &value);
    g_cached_weather.temp_min = (int)value;

    len = sizeof(g_cached_weather.desc);
    hw_nvs_get_string(NS_WEATHER, "desc", g_cached_weather.desc, &len);

    len = sizeof(g_cached_weather.icon);
    hw_nvs_get_string(NS_WEATHER, "icon", g_cached_weather.icon, &len);

    hw_nvs_get_int(NS_WEATHER, "humidity", &value);
    g_cached_weather.humidity = (int)value;

    len = sizeof(buf);
    if (hw_nvs_get_string(NS_WEATHER, "wind", buf, &len) == 0) {
        g_cached_weather.wind_speed = atof(buf);
    }

    hw_nvs_get_int(NS_WEATHER, "sr_h", &value);
    g_cached_weather.sunrise_hour = (int)value;
    hw_nvs_get_int(NS_WEATHER, "sr_m", &value);
    g_cached_weather.sunrise_minute = (int)value;
    hw_nvs_get_int(NS_WEATHER, "ss_h", &value);
    g_cached_weather.sunset_hour = (int)value;
    hw_nvs_get_int(NS_WEATHER, "ss_m", &value);
    g_cached_weather.sunset_minute = (int)value;

    hw_nvs_get_int(NS_WEATHER, "mr_h", &value);
    g_cached_weather.moonrise_hour = (int)value;
    hw_nvs_get_int(NS_WEATHER, "mr_m", &value);
    g_cached_weather.moonrise_minute = (int)value;
    hw_nvs_get_int(NS_WEATHER, "ms_h", &value);
    g_cached_weather.moonset_hour = (int)value;
    hw_nvs_get_int(NS_WEATHER, "ms_m", &value);
    g_cached_weather.moonset_minute = (int)value;

    len = sizeof(g_cached_weather.moon_phase);
    hw_nvs_get_string(NS_WEATHER, "moon", g_cached_weather.moon_phase, &len);
    len = sizeof(g_cached_weather.moon_phase_icon);
    hw_nvs_get_string(NS_WEATHER, "moon_i", g_cached_weather.moon_phase_icon, &len);

    g_weather_cached = true;
    ESP_LOGI(TAG, "Weather loaded: %d°C, %s", g_cached_weather.temp, g_cached_weather.desc);
    return 0;
}

int app_nvs_get_weather_data(weather_data_t* weather) {
    if (weather == NULL || !g_weather_cached) return -1;
    *weather = g_cached_weather;
    return 0;
}

void app_nvs_set_weather(const weather_data_t* weather) {
    if (weather == NULL) return;
    g_cached_weather = *weather;
    g_weather_cached = true;
}

int app_nvs_save_all(void) {
    ESP_LOGI(TAG, "Saving all data to NVS...");

    int ret = 0;

    if (app_nvs_save_cameras() != 0) {
        ESP_LOGE(TAG, "Failed to save cameras");
        ret = -1;
    }

    if (app_nvs_save_lens() != 0) {
        ESP_LOGE(TAG, "Failed to save lens");
        ret = -1;
    }

    if (app_nvs_save_ui_state() != 0) {
        ESP_LOGE(TAG, "Failed to save UI state");
        ret = -1;
    }

    if (app_nvs_save_wifi() != 0) {
        ESP_LOGE(TAG, "Failed to save WiFi");
        ret = -1;
    }

    if (app_nvs_save_location() != 0) {
        ESP_LOGE(TAG, "Failed to save location");
        ret = -1;
    }

    if (app_nvs_save_weather() != 0) {
        ESP_LOGW(TAG, "Failed to save weather");
    }

    if (ret == 0) {
        ESP_LOGI(TAG, "All data saved successfully");
    }

    return ret;
}

int app_nvs_load_all(void) {
    ESP_LOGI(TAG, "Loading all data from NVS...");

    int ret = 0;

    if (app_nvs_load_wifi() != 0) {
        ESP_LOGW(TAG, "No WiFi config found");
    }

    if (app_nvs_load_location() != 0) {
        ESP_LOGW(TAG, "No location data found");
    }

    if (app_nvs_load_weather() != 0) {
        ESP_LOGW(TAG, "No weather data found");
    }

    if (app_nvs_load_cameras() != 0) {
        ESP_LOGW(TAG, "No camera data found");
    }

    if (app_nvs_load_lens() != 0) {
        ESP_LOGW(TAG, "No lens data found");
    }

    if (app_nvs_load_ui_state() != 0) {
        ESP_LOGW(TAG, "No UI state found");
    }

    ESP_LOGI(TAG, "All data loaded");
    return ret;
}
