#ifndef __HW_SDCARD_H__
#define __HW_SDCARD_H__

#include "driver/gpio.h"
#include <string.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdmmc_host.h"


#define BSP_SD_CLK      GPIO_NUM_5
#define BSP_SD_CMD      GPIO_NUM_6
#define BSP_SD_D0       GPIO_NUM_7

extern sdmmc_card_t *card;

esp_err_t hw_sd_init(void);

#endif // !__BSP_SD_H