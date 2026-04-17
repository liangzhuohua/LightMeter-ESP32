#ifndef __MAX17055_H__
#define __MAX17055_H__

#include <stddef.h>
#include <i2cdev.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX17055_I2C_ADDR (0x36)

#define MAX17055_STATUS_POR_MASK  (1 << 1)
#define MAX17055_STATUS_BST_MASK  (1 << 3)
#define MAX17055_POR_CLEAR_MASK   (0xFFFD)

#define MAX17055_FSTAT_DNR_MASK   (1 << 0)

#define MAX17055_MODELCFG_REFRESH_MASK (1 << 15)

#define MAX17055_VCELL_LSB_UV       78.125f
#define MAX17055_CURRENT_LSB_UV      1.5625f
#define MAX17055_CAPACITY_LSB_UVH    5.0f
#define MAX17055_TTE_TTF_LSB_S       5.625f

typedef enum {
    MAX17055_REG_STATUS      = 0x00,
    MAX17055_REG_VALRTTH     = 0x01,
    MAX17055_REG_TALRTTH     = 0x02,
    MAX17055_REG_SALRTTH     = 0x03,
    MAX17055_REG_ATRATE      = 0x04,
    MAX17055_REG_REPCAP      = 0x05,
    MAX17055_REG_REPSOC      = 0x06,
    MAX17055_REG_AGE         = 0x07,
    MAX17055_REG_TEMP        = 0x08,
    MAX17055_REG_VCELL       = 0x09,
    MAX17055_REG_CURRENT     = 0x0A,
    MAX17055_REG_AVGCURRENT  = 0x0B,
    MAX17055_REG_QRESIDUAL   = 0x0C,
    MAX17055_REG_MIXSOC      = 0x0D,
    MAX17055_REG_AVSOC       = 0x0E,
    MAX17055_REG_MIXCAP      = 0x0F,
    MAX17055_REG_FULLCAPREP  = 0x10,
    MAX17055_REG_TTE         = 0x11,
    MAX17055_REG_QRTABLE00   = 0x12,
    MAX17055_REG_FULLSOCTHR  = 0x13,
    MAX17055_REG_RCELL       = 0x14,
    MAX17055_REG_AVGTA       = 0x16,
    MAX17055_REG_CYCLES      = 0x17,
    MAX17055_REG_DESIGNCAP   = 0x18,
    MAX17055_REG_AVGVCELL    = 0x19,
    MAX17055_REG_MAXMINVOLT  = 0x1B,
    MAX17055_REG_MAXMINCURR  = 0x1C,
    MAX17055_REG_CONFIG      = 0x1D,
    MAX17055_REG_ICHGTERM    = 0x1E,
    MAX17055_REG_AVCAP       = 0x1F,
    MAX17055_REG_TTF         = 0x20,
    MAX17055_REG_DEVNAME     = 0x21,
    MAX17055_REG_QRTABLE10   = 0x22,
    MAX17055_REG_FULLCAPNOM  = 0x23,
    MAX17055_REG_AIN         = 0x27,
    MAX17055_REG_LEARNCFG    = 0x28,
    MAX17055_REG_FILTERCFG   = 0x29,
    MAX17055_REG_RELAXCFG    = 0x2A,
    MAX17055_REG_MISCCFG     = 0x2B,
    MAX17055_REG_TGAIN       = 0x2C,
    MAX17055_REG_TOFF        = 0x2D,
    MAX17055_REG_CGAIN       = 0x2E,
    MAX17055_REG_COFF        = 0x2F,
    MAX17055_REG_QRTABLE20   = 0x32,
    MAX17055_REG_DIETEMP     = 0x34,
    MAX17055_REG_FULLCAP     = 0x35,
    MAX17055_REG_RCOMP0      = 0x38,
    MAX17055_REG_TEMPCO      = 0x39,
    MAX17055_REG_VEMPTY      = 0x3A,
    MAX17055_REG_FSTAT       = 0x3D,
    MAX17055_REG_TIMER       = 0x3E,
    MAX17055_REG_SHDNTIMER   = 0x3F,
    MAX17055_REG_QRTABLE30   = 0x42,
    MAX17055_REG_RGAIN       = 0x43,
    MAX17055_REG_DQACC       = 0x45,
    MAX17055_REG_DPACC       = 0x46,
    MAX17055_REG_CONVGCFG    = 0x49,
    MAX17055_REG_VFREMCAP    = 0x4A,
    MAX17055_REG_QH          = 0x4D,
    MAX17055_REG_STATUS2     = 0xB0,
    MAX17055_REG_POWER       = 0xB1,
    MAX17055_REG_AVGPOWER    = 0xB3,
    MAX17055_REG_IALRTTH     = 0xB4,
    MAX17055_REG_TTFCFG      = 0xB5,
    MAX17055_REG_CVMIXCAP    = 0xB6,
    MAX17055_REG_CVHALFTIME  = 0xB7,
    MAX17055_REG_CGTEMPCO    = 0xB8,
    MAX17055_REG_CURVE       = 0xB9,
    MAX17055_REG_HIBCFG      = 0xBA,
    MAX17055_REG_CONFIG2     = 0xBB,
    MAX17055_REG_VRIPPLE     = 0xBC,
    MAX17055_REG_RIPPLECFG   = 0xBD,
    MAX17055_REG_TIMERH      = 0xBE,
    MAX17055_REG_RSENSE      = 0xD0,
    MAX17055_REG_SCOCVLIM    = 0xD1,
    MAX17055_REG_SOCHOLD     = 0xD3,
    MAX17055_REG_MAXPEAKPWR  = 0xD4,
    MAX17055_REG_SUSPEAKPWR  = 0xD5,
    MAX17055_REG_PACKRES     = 0xD6,
    MAX17055_REG_SYSRES      = 0xD7,
    MAX17055_REG_MINSYSVOLT  = 0xD8,
    MAX17055_REG_MPPCURR     = 0xD9,
    MAX17055_REG_SPPCURR     = 0xDA,
    MAX17055_REG_MODELCFG    = 0xDB,
    MAX17055_REG_ATQRES      = 0xDC,
    MAX17055_REG_ATTTE       = 0xDD,
    MAX17055_REG_ATAVSOC     = 0xDE,
    MAX17055_REG_ATAVCAP     = 0xDF,
    MAX17055_REG_OCV         = 0xFB,
    MAX17055_REG_VFSOC       = 0xFF,
} max17055_reg_t;

typedef struct {
    uint16_t design_cap;
    uint16_t ichg_term;
    uint16_t vempty;
    float vcharge;
    float rsense_mohm;
} max17055_config_t;

esp_err_t max17055_init_desc(i2c_dev_t *dev, i2c_port_t port, gpio_num_t sda_gpio, gpio_num_t scl_gpio);

esp_err_t max17055_free_desc(i2c_dev_t *dev);

esp_err_t max17055_probe(i2c_dev_t *dev);

esp_err_t max17055_init(i2c_dev_t *dev, const max17055_config_t *cfg);

esp_err_t max17055_get_vcell(i2c_dev_t *dev, float *voltage_mv);

esp_err_t max17055_get_avg_vcell(i2c_dev_t *dev, float *voltage_mv);

esp_err_t max17055_get_current(i2c_dev_t *dev, float *current_ma);

esp_err_t max17055_get_avg_current(i2c_dev_t *dev, float *current_ma);

esp_err_t max17055_get_soc(i2c_dev_t *dev, float *soc_pct);

esp_err_t max17055_get_rep_cap(i2c_dev_t *dev, float *cap_mah);

esp_err_t max17055_get_temperature(i2c_dev_t *dev, float *temp_c);

esp_err_t max17055_get_tte(i2c_dev_t *dev, float *tte_s);

esp_err_t max17055_get_ttf(i2c_dev_t *dev, float *ttf_s);

esp_err_t max17055_get_cycles(i2c_dev_t *dev, uint16_t *cycles);

esp_err_t max17055_get_age(i2c_dev_t *dev, uint8_t *age_pct);

esp_err_t max17055_get_full_cap(i2c_dev_t *dev, float *cap_mah);

esp_err_t max17055_get_status(i2c_dev_t *dev, uint16_t *status);

esp_err_t max17055_set_valrt(i2c_dev_t *dev, float v_min_mv, float v_max_mv);

esp_err_t max17055_set_salrt(i2c_dev_t *dev, uint8_t soc_min, uint8_t soc_max);

esp_err_t max17055_set_ialrt(i2c_dev_t *dev, float i_min_ma, float i_max_ma);

#ifdef __cplusplus
}
#endif

#endif
