idf_component_register(
    SRCS "main.c"
    "bsp_i2c_init.c"
    "hw_oled.c"
    "hw_sdcard.c"
    "hw_veml7700.c"
    "app_exposure_calc.c"
    "app_ui.c"
    "img_cam.c"
    "img_len.c"
    "app_controller.c"
    "app_ui_calc_port.c"
    "img_auto_mode.c"
    "img_landscape_mode.c"
    "img_manual_mode.c"
    "img_portrait_mode.c"
    "hw_wifi.c"
    "app_ui_wifi_port.c"

    INCLUDE_DIRS ".")

set_source_files_properties(
    ${LV_DEMOS_SOURCES}
    PROPERTIES COMPILE_OPTIONS
    -DLV_LVGL_H_INCLUDE_SIMPLE)
