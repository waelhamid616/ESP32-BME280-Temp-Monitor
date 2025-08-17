/*
 * ESP32 + BME280 Demo (main)
 * Scans I2C, initializes the BME280, reads T/P/H once per second, and prints.
 * Intended for ESP-IDF; uses vTaskDelayUntil() for drift-free timing.
 * Author: Wael Hamid  |  Date: 2025-08-09
 */
#include <stdio.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/i2c.h"    // scan done in main 

#include "bme280.h"       // driver public API (macros + prototypes)

#include "wifi.h"
#include "http_server.h"
#include "http_client_ext.h"
#include <math.h>   // for NAN

static const char *TAG = "APP_MAIN"; // for logs inside app_main.c

// start here 
static weather_t g_outside = { NAN, NAN };


static void outside_temp_task(void *arg)
{
    while (1) {
        g_outside = fetch_outside_current();      // HTTPS API call (Open-Meteo)
        vTaskDelay(pdMS_TO_TICKS(6000));          // update every 6 sec
    }
}


/*
 * Function: app_main
 * ESP-IDF application entry point.
 * - Initializes Wi-Fi, and HTTP server
 * - Launches task to fetch outside weather
 * - Scans I2C bus and initializes BME280 sensor
 * - Continuously reads T/P/H every ~1s and updates web page
 */
void app_main(void)
{
    
    // 0. Bring up Wi-Fi, time, and web server ===
    ESP_ERROR_CHECK(wifi_start_station());    // connect to router (logs GOT_IP)
    web_start();                             // starts HTTP server at "/"

    // Start the background task that fetches outside temperature
    xTaskCreate(outside_temp_task, "outside_temp_task", 4096, NULL, 5, NULL);


    // 1. Call the i2c initilaizer 
    ESP_ERROR_CHECK(bme_i2c_master_init());
    ESP_LOGI(TAG, "Starting I2C scan...");

    // iterate through the addresses until u get back the sensor address
     for (uint8_t address= 0x03; address <= 0x77; address++){

        //create an empty command for setup 
        i2c_cmd_handle_t cmd = i2c_cmd_link_create();
        //add a start condition ( SDA H -> L)
        i2c_master_start(cmd);
        // send address with start bit 0 (write)
        i2c_master_write_byte(cmd,(address<<1) | I2C_MASTER_WRITE,true); // make space for the write bit 
        i2c_master_stop(cmd);
        // run the transaction for short time 
        esp_err_t ret= i2c_master_cmd_begin(I2C_PORT,cmd,pdMS_TO_TICKS(50));
        i2c_cmd_link_delete(cmd);

        if (ret == ESP_OK){

            printf ("Found Sensor at 0x%02X",address); //02 is for two chars & X is for hex 
        }
    }
    ESP_LOGI(TAG, "I2C scan complete.");

    // 2. Call rhe bme2800 sensor initializer
    ESP_ERROR_CHECK(bme280_init());

    // 3. Read calibration T,P,H constants first  
    ESP_ERROR_CHECK(bme280_read_calibration());

    // 4. configure control registes to set measuremnt standards 
    ESP_ERROR_CHECK(bme280_config_normal());

    /* 5. start polling and allow the sensor to send the data... 
    -operating in normal mode */

    // period = t_standby (1000 ms) + conv time (~30 ms) ≈ 1030 ms
    const TickType_t period_ticks = pdMS_TO_TICKS(1030);
    TickType_t last_wake = xTaskGetTickCount();
    while(1){

        int32_t raw_T,raw_P,raw_H;
        ESP_ERROR_CHECK(bme280_read_raw(&raw_T, &raw_P, &raw_H)); //read the raw data

        //float path (datasheet-style double) — simpler to print:
        double T_C  = BME280_compensate_T_double(raw_T);   // °C
        double P_Pa = BME280_compensate_P_double(raw_P);  // Pa
        double H_RH = bme280_compensate_H_double(raw_H); // %RH
        printf("T=%.2f °C  P=%.2f hPa  H=%.1f %%RH\n", T_C, P_Pa/100.0, H_RH);
        //publish latest readings to the web page ===
        web_set_readings((float)T_C, g_outside.temp,(float)H_RH, g_outside.humid);

        vTaskDelayUntil(&last_wake, period_ticks); // wait until last_wake + period_ticks, adjusting for time already spent
    }
}
