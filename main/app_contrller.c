#include "app_contrller.h"
#include "app_ui_calc_port.h"
#include "app_ui.h"
#include "app_exposure_calc.h"
#include <stdio.h>

void app_contrller_init(void)
{
    CAM cam = ui_calc_port_extract_cam_from_card(app_ui_get_cam_selected_card());
    LEN len = ui_calc_port_extract_len_from_card(app_ui_get_len_selected_card());
    

    if (cam.shutter_stop_count != 0 && len.aperture_stop_count != 0)
    {
        printf("cam_shutter_stop_count:%d\n", cam.shutter_stop_count);
        printf("len_aperture_stop_count:%d\n", len.aperture_stop_count);
        printf("len_focal_length:%f\n", len.focal_length);
        printf("cam_flash_sync_shutter:%f\n", cam.flash_sync_shutter);
        for(int i =  0; i < cam.shutter_stop_count; i++)
        {
            printf("cam_shutter_stopops[%d]:%f\n", i, cam.shutter_stops[i]);
        }
        for(int i =  0; i < len.aperture_stop_count; i++)
        {
            printf("len_aperture_stopops[%d]:%f\n", i, len.aperture_stops[i]);
        }
        // 直接使用相机和镜头的自定义数组
        // ui_calc_port_set_shutter_to_roller(main_roller_shutter, 1.0f/500.0f, cam.shutter_stops, cam.shutter_stop_count);
        // ui_calc_port_set_aperture_to_roller(main_roller_aperture, 2.8f, len.aperture_stops, len.aperture_stop_count);
        
    }
    
}