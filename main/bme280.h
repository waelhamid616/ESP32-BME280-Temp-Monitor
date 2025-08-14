
//.h file starts here 

/*
 * BME280 driver – public API
 * Author: Wael Hamid
 * Date: Aug 9, 2025
 */

#ifndef BME280_H
#define BME280_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/i2c.h"   // for I2C_NUM_0 used in macros

//define pinout numbes and I2c constants 
#define SDA_GPIO 21
#define SCL_GPIO 22 
#define I2C_PORT I2C_NUM_0   // I2c Controller 0
#define I2C_HZ 100000       //100 khz clock 

//define BME280 Sensor Constants 
#define BME280_ADDR       0x77   // your scan showed 0x77
#define BME280_REG_ID     0xD0   // chip ID register
#define BME280_CHIP_ID    0x60   // expected value for BME280
#define BME280_REG_RESET  0xE0   // soft reset register
#define BME280_RESET_CMD  0xB6   // soft reset command
#define BME280_REG_STATUS 0xF3   // status register

#define CTRL_HUM  0xF2          // humdidty register control 
#define CTRL_VAL1 0x03         // x4 oversampling for humidity
#define CTRL_MEAS 0xF4        // Temp & Pressure control 
#define CTRL_VAL2 0x6F       // x4 oversampling for Temp and press and enable normal mode 
#define CTRL_CONF 0xF5      // config register to select stand by time and enable IRR Filter 
#define CTRL_VAL3 0xA8     // 500ms stanby time, ebnable IRR and disable SPI

//structure to store temp,press, & humididty calibration coeffs (Table#16 in BME280 Datasheet)
 typedef struct {
    
    uint16_t dig_T1; int16_t dig_T2; int16_t dig_T3;   // Tempearture coeffs 
    uint16_t dig_P1; int16_t dig_P2; int16_t dig_P3;  // Pressure coeffs 
    int16_t dig_P4; int16_t dig_P5; int16_t dig_P6; 
    int16_t dig_P7; int16_t dig_P8; int16_t dig_P9; 
    uint8_t dig_H1; int16_t dig_H2; uint8_t dig_H3;  // Humidity coeffs 
    int16_t dig_H4; int16_t dig_H5; int8_t  dig_H6;

} bme280_calib_t;

// Bosch-style typedef used by the "double" compensators
typedef int32_t BME280_S32_t;

// ===== Public API (no 'static' here) =====
esp_err_t bme_i2c_master_init(void);
esp_err_t bme280_init(void);
esp_err_t bme280_read_calibration(void);
esp_err_t bme280_config_normal(void);

esp_err_t bme280_read_raw(int32_t *adc_T, int32_t *adc_P, int32_t *adc_H);

double BME280_compensate_T_double(BME280_S32_t adc_T);   // °C
double BME280_compensate_P_double(BME280_S32_t adc_P);   // Pa
double bme280_compensate_H_double(BME280_S32_t adc_H);   // %RH

#endif // BME280_H
