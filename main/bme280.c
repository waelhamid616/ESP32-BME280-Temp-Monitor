/*
 * BME280 Driver (implementation)
 * I2C transactions, init/reset, calibration reads, raw reads, and compensation
 * (double-precision) per Bosch datasheet. Private helpers kept file-local.
 * Author: Wael Hamid  |  Date: 2025-08-09
 */


#include "bme280.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "BME280"; // for logs inside bme280.c

static bme280_calib_t calib;         // private globals stay static in .c
static BME280_S32_t t_fine = 0;

// ---- private helpers ----
static esp_err_t i2c_write_u8(uint8_t device_addr, uint8_t register_addr, uint8_t val);
static esp_err_t i2c_read_bytes(uint8_t device_addr, uint8_t register_addr, uint8_t *buffer, size_t len);
static int16_t   sign_extend_12(uint16_t v);

/**
 * @brief Initialize I2C master interface for ESP32.
 *
 * Configures GPIO pins, I2C mode, and clock speed for the ESP32 I2C master.
 * Installs the I2C driver on the specified port. Must be called before any I2C transactions.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t bme_i2c_master_init (void){

    i2c_config_t conf = {

        .mode = I2C_MODE_MASTER,
        .sda_io_num = SDA_GPIO,
        .scl_io_num = SCL_GPIO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_HZ
    };
    ESP_ERROR_CHECK(i2c_param_config(I2C_PORT,&conf));
    ESP_ERROR_CHECK(i2c_driver_install(I2C_PORT,conf.mode,0,0,0));
    return ESP_OK;
    
}

/**
 * @brief Write a single byte to a register on an I2C device.
 *
 * Sends a register address followed by a data byte over I2C to the specified device address.
 * Waits up to 100 ms for the transaction to complete.
 *
 * @param device_addr 7-bit I2C device address.
 * @param register_addr Register address to write to.
 * @param val Byte value to write.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t i2c_write_u8(uint8_t device_addr, uint8_t register_addr,uint8_t val){

    //create an empty command for setup 
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd); // satrt the i2c message 
    i2c_master_write_byte(cmd, (device_addr<<1) | I2C_MASTER_WRITE, true); // write device addr & expect ACK
    i2c_master_write_byte(cmd,register_addr,true);    // send register index, expect ACK
    i2c_master_write_byte(cmd,val,true);    // send data byte, expect ACK
    i2c_master_stop(cmd);

    esp_err_t ret= i2c_master_cmd_begin(I2C_PORT,cmd, pdMS_TO_TICKS(100)); // actaully beigin writting
    i2c_cmd_link_delete(cmd);                                             // free the command list

    return ret;
}

/**
 * @brief Read multiple bytes from a register on an I2C device.
 *
 * Sends a register address, then reads a specified number of bytes into a buffer.
 * Performs a repeated start condition between write and read phases.
 *
 * @param device_addr 7-bit I2C device address.
 * @param register_addr Register address to read from.
 * @param buffer Pointer to destination buffer.
 * @param len Number of bytes to read.
 * @return ESP_OK on success, or an error code on failure.
 */
static esp_err_t i2c_read_bytes(uint8_t device_addr, uint8_t register_addr, uint8_t *buffer, size_t len){

    //create an empty command for setup 
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd); // satrt the i2c message 
    i2c_master_write_byte(cmd, (device_addr<<1) | I2C_MASTER_WRITE, true); // write device addr & expect ACK
    i2c_master_write_byte(cmd,register_addr,true);    // send register index, expect ACK
    i2c_master_start(cmd); // satrt again while keeping the register address 
    i2c_master_write_byte(cmd, (device_addr << 1) | I2C_MASTER_READ, true); //I’m now requesting to read from you slave.

    //check there are multiple bytes to read
    if (len > 1){

        //read & store all bytes excpet the last one...
        i2c_master_read(cmd, buffer, len-1, I2C_MASTER_ACK);       
        }
    i2c_master_read_byte(cmd, &buffer[len - 1], I2C_MASTER_NACK); // read last byte, NACK
    i2c_master_stop(cmd);
    
    esp_err_t ret= i2c_master_cmd_begin(I2C_PORT,cmd, pdMS_TO_TICKS(100)); // actaully beigin writting
    i2c_cmd_link_delete(cmd);                                             // free the command list

    return ret;
}

/**
 * @brief Initialize the BME280 sensor.
 *
 * Reads and verifies the chip ID, performs a soft reset, and waits for calibration registers to be ready.
 * Must be called before reading calibration data or configuring the sensor.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
 esp_err_t bme280_init(void){

    uint8_t id = 0; // to store the chip id 

    // 1- read chip id for the id register  
    ESP_ERROR_CHECK(i2c_read_bytes(BME280_ADDR,BME280_REG_ID,&id,1));
    printf("BME280 CHIP_ID read: 0x%02X\n", id);

    if (id !=BME280_CHIP_ID ){
        printf("ERROR: Unexpected CHIP_ID (expected 0x60). Check wiring or address.\n");
        return ESP_FAIL;
    }
    printf("OK: BME280 detected.\n");

    //test out the tick eate 
    printf("Tick rate: %lu Hz, 1 tick = %u ms\n",(unsigned long)configTICK_RATE_HZ,(unsigned)portTICK_PERIOD_MS);

    // 2- perform soft reset 
    ESP_ERROR_CHECK(i2c_write_u8(BME280_ADDR,BME280_REG_RESET,BME280_RESET_CMD));
    vTaskDelay(pdMS_TO_TICKS(1)); // wait >2ms per datasheet

    // 3- check that the status register bits are setting and resetting properly 
    while (1){
        uint8_t status=0;
        ESP_ERROR_CHECK(i2c_read_bytes(BME280_ADDR,BME280_REG_STATUS,&status,1));
        if((status & 0x01) ==0){
            break;// bit0 im_update gets reset indicating calibration regs are ready 
        } 
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    printf("BME280 ready: calibration registers loaded.\n");
    return ESP_OK;
}


/**
 * @brief Read BME280 temperature, pressure, and humidity calibration constants.
 *
 * Reads the calibration registers as specified in the BME280 datasheet and stores the values
 * in the global calib structure. Required for compensation functions to work.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
 esp_err_t bme280_read_calibration(void){

    //next, store these coffes temp+pressure , &humidity 

    // Read 26 bytes from register 0x88 to 0xA1:
    //   - 0x88..0x9F → Temp & Pressure calibration (T1..T3, P1..P9)
    //   - 0xA0       → Reserved
    //   - 0xA1       → Humidity calibration H1
    uint8_t buf1[26];
    ESP_ERROR_CHECK(i2c_read_bytes(BME280_ADDR, 0x88, buf1, 26));

    // -------- Temperature calibration --------
    calib.dig_T1= (uint16_t)((buf1[1]<<8) | buf1[0]);  // 0x88 (LSB), 0x89 (MSB), unsigned
    calib.dig_T2= (int16_t)((buf1[3]<<8) | buf1[2]);  // 0x8A (LSB), 0x8B (MSB), signed
    calib.dig_T3= (int16_t)((buf1[5]<<8) | buf1[4]); // 0x8C (LSB), 0x8D (MSB), signed

    // -------- Pressure calibration --------
    calib.dig_P1 = (uint16_t)(buf1[7] << 8 | buf1[6]);  // 0x8E, 0x8F, unsigned
    calib.dig_P2 = (int16_t)(buf1[9] << 8 | buf1[8]);   // 0x90, 0x91, signed
    calib.dig_P3 = (int16_t)(buf1[11] << 8 | buf1[10]); // 0x92, 0x93, signed
    calib.dig_P4 = (int16_t)(buf1[13] << 8 | buf1[12]); // 0x94, 0x95, signed
    calib.dig_P5 = (int16_t)(buf1[15] << 8 | buf1[14]); // 0x96, 0x97, signed
    calib.dig_P6 = (int16_t)(buf1[17] << 8 | buf1[16]); // 0x98, 0x99, signed
    calib.dig_P7 = (int16_t)(buf1[19] << 8 | buf1[18]); // 0x9A, 0x9B, signed
    calib.dig_P8 = (int16_t)(buf1[21] << 8 | buf1[20]); // 0x9C, 0x9D, signed
    calib.dig_P9 = (int16_t)(buf1[23] << 8 | buf1[22]); // 0x9E, 0x9F, signed


    // -------- Humidity calibration (part 1) --------
    // buf1[24] = 0xA0 → reserved (ignore)
    calib.dig_H1 = buf1[25];
    // -------- Humidity calibration (part 2) --------
    uint8_t buf2[7];
    ESP_ERROR_CHECK(i2c_read_bytes(BME280_ADDR, 0xE1, buf2, 7));

    calib.dig_H2= (int16_t)((buf2[1]<<8) | buf2[0]);    // 0x8A (LSB), 0x8B (MSB), signed
    calib.dig_H3= buf2[2];                             // 0xE3, unsigned
    // ^ Shift E4 left by 4 to make room for the low nibble of E5,
    //   then OR in E5's lowest 4 bits to form a 12-bit number in bits 11..0.
    uint16_t raw_h4 = ((uint16_t)buf2[3] << 4) | (buf2[4] & 0x0F);

    // ^ Shift E6 left by 4 to make room for E5's high nibble,
    //   then OR in E5's top 4 bits. Now raw_h5 also holds 12 bits in 11..0.
    uint16_t raw_h5 = ((uint16_t)buf2[5] << 4) | (buf2[4] >> 4);

    calib.dig_H4 = sign_extend_12(raw_h4);  // convert packed 12-bit to proper int16_t
    calib.dig_H5 = sign_extend_12(raw_h5);  // (handles negative values correctly)
    calib.dig_H6 = (int8_t)buf2[6]; // 0xE7, signed char

    return ESP_OK;
    }


/**
 * @brief Sign-extend a 12-bit value to 16 bits.
 *
 * Handles two’s complement conversion for BME280 humidity calibration coefficients
 * that are stored as packed 12-bit signed values.
 *
 * @param v 12-bit unsigned value to convert.
 * @return int16_t Sign-extended value.
 */
static int16_t sign_extend_12(uint16_t v)
{
    // Bit 11 is the sign bit for a 12-bit number (bits 11..0).
    // If it's set, we need to fill the upper 4 bits (15..12) with 1s
    // so the value remains negative when treated as a 16-bit signed int.
    if (v & 0x0800)             // 0x0800 = 0000 1000 0000 0000 (bit 11)
        v |= 0xF000;           // set bits 15..12 to 1 to preserve the sign

    return (int16_t)v;         // now safe to interpret as signed 16-bit
 }

 /**
 * @brief Configure BME280 to normal measurement mode.
 *
 * Sets oversampling for humidity, temperature, and pressure,
 * applies standby time and IIR filter settings.
 *
 * @return ESP_OK on success, or an error code on failure.
 */
 esp_err_t bme280_config_normal(void){

        // 1.Start by configuring Humidity measuremnt
        ESP_ERROR_CHECK(i2c_write_u8(BME280_ADDR, CTRL_HUM,  CTRL_VAL1));   
        
        // 2.Next  configure pressure & temp & set sensor in normal mode 
        ESP_ERROR_CHECK(i2c_write_u8(BME280_ADDR, CTRL_MEAS, CTRL_VAL2)); 

        // 3. select the standby time (off time)
        ESP_ERROR_CHECK(i2c_write_u8(BME280_ADDR, CTRL_CONF, CTRL_VAL3)); 

       return ESP_OK;
    }

/**
 * @brief Read raw ADC values for temperature, pressure, and humidity.
 *
 * Performs a burst read from the BME280’s measurement registers to retrieve all
 * sensor values from the same measurement cycle.
 *
 * @param adc_T Pointer to store raw temperature ADC value.
 * @param adc_P Pointer to store raw pressure ADC value.
 * @param adc_H Pointer to store raw humidity ADC value.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t bme280_read_raw(int32_t *adc_T, int32_t *adc_P, int32_t *adc_H)
{

    // declare an array for the 8 addresses that will store the raw data     
    uint8_t d[8];

    // 0xF7..0xFE -> P_msb, P_lsb, P_xlsb, T_msb, T_lsb, T_xlsb, H_msb, H_lsb
    ESP_ERROR_CHECK(i2c_read_bytes(BME280_ADDR,0xF7,d,sizeof(d)));

    // 20‑bit unsigned: [msb:8][lsb:8][xlsb:upper4]
    *adc_P = (int32_t)((((uint32_t)d[0] << 12) | ((uint32_t)d[1] << 4) | (d[2] >> 4)));
    *adc_T = (int32_t)((((uint32_t)d[3] << 12) | ((uint32_t)d[4] << 4) | (d[5] >> 4)));
    *adc_H = (int32_t)(((uint32_t)d[6] << 8) | d[7]);
    /*Note: they do get converted to signed 32 bit since the datsheet ask for that for it to 
    work in the conversion eautions provided by it. */

    return ESP_OK;
}

/**
 * @brief Convert raw temperature reading to degrees Celsius (double precision).
 *
 * Uses the BME280 datasheet's floating-point compensation algorithm.
 * Updates the global t_fine variable for use in pressure/humidity compensation.
 *
 * @param adc_T Raw ADC temperature value.
 * @return Temperature in degrees Celsius.
 */
double BME280_compensate_T_double(BME280_S32_t adc_T)
{
    double var1, var2, T;

    var1 = ((double)adc_T / 16384.0 - (double)calib.dig_T1 / 1024.0) * (double)calib.dig_T2;
    var2 = (((double)adc_T / 131072.0 - (double)calib.dig_T1 / 8192.0) *
            ((double)adc_T / 131072.0 - (double)calib.dig_T1 / 8192.0)) * (double)calib.dig_T3;

    t_fine = (BME280_S32_t)(var1 + var2);
    T = (var1 + var2) / 5120.0;

    return T;
}

/**
 * @brief Convert raw pressure reading to Pascals (double precision).
 *
 * Uses the BME280 datasheet's floating-point compensation algorithm.
 * Requires t_fine to be set by a temperature compensation call first.
 *
 * @param adc_P Raw ADC pressure value.
 * @return Pressure in Pascals.
 */
double BME280_compensate_P_double(BME280_S32_t adc_P)
{
    double var1, var2, p;

    var1 = ((double)t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * (double)calib.dig_P6 / 32768.0;
    var2 = var2 + var1 * (double)calib.dig_P5 * 2.0;
    var2 = (var2 / 4.0) + (double)calib.dig_P4 * 65536.0;
    var1 = ((double)calib.dig_P3 * var1 * var1 / 524288.0 +
            (double)calib.dig_P2 * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * (double)calib.dig_P1;

    if (var1 == 0.0)
        return 0; // avoid division by zero

    p = 1048576.0 - (double)adc_P;
    p = (p - (var2 / 4096.0)) * 6250.0 / var1;
    var1 = (double)calib.dig_P9 * p * p / 2147483648.0;
    var2 = p * (double)calib.dig_P8 / 32768.0;
    p = p + (var1 + var2 + (double)calib.dig_P7) / 16.0;

    return p;
}

/**
 * @brief Convert raw humidity reading to %RH (double precision).
 *
 * Uses the BME280 datasheet's floating-point compensation algorithm.
 * Requires t_fine to be set by a temperature compensation call first.
 *
 * @param adc_H Raw ADC humidity value.
 * @return Relative humidity in %RH.
 */
double bme280_compensate_H_double(BME280_S32_t adc_H)
{
    double var_H;

    var_H = (double)t_fine - 76800.0;
    var_H = (adc_H - ((double)calib.dig_H4 * 64.0 +
                      (double)calib.dig_H5 / 16384.0 * var_H)) *
            ((double)calib.dig_H2 / 65536.0 *
             (1.0 + (double)calib.dig_H6 / 67108864.0 * var_H *
                    (1.0 + (double)calib.dig_H3 / 67108864.0 * var_H)));

    var_H = var_H * (1.0 - (double)calib.dig_H1 * var_H / 524288.0);

    if (var_H > 100.0) var_H = 100.0;
    else if (var_H < 0.0) var_H = 0.0;

    return var_H;
}