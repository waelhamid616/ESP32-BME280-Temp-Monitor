/*
 * ESP32 Smart Climate Monitor (main)
 * - Connects to Wi-Fi and synchronizes time (SNTP).
 * - Starts a local HTTP server to display live readings.
 * - Spawns a background task to fetch outside weather (Open-Meteo API).
 * - Scans the I2C bus, initializes the BME280, and reads T/P/H once per second.
 * - Publishes readings to the web page and evaluates SMS alerts via Twilio.
 * Uses vTaskDelayUntil() for drift-free timing in FreeRTOS.
 *
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
#include "alert_eval.h"
#include "sms_client.h"

#include "wifi.h"
#include "http_server.h"
#include "http_client_ext.h"
#include <math.h>   // for NAN

#include <time.h>
#include "esp_sntp.h"
#include "esp_netif.h" //netword interface 

static const char *TAG = "APP_MAIN"; // for logs inside app_main.c

// start here 
static weather_t g_outside = { NAN, NAN };

/**
 * @brief Background task that fetches outside temperature and humidity.
 *
 * Periodically queries the Open-Meteo HTTPS API using fetch_outside_current(),
 * updates the global g_outside struct, and sleeps for ~6 seconds between polls.
 *
 * @param arg Unused (reserved for FreeRTOS task parameter).
 * @return None (task runs indefinitely).
 */
static void outside_temp_task(void *arg)
{
    while (1) {
        g_outside = fetch_outside_current();      // HTTPS API call (Open-Meteo)
        vTaskDelay(pdMS_TO_TICKS(6000));         // update every 6 sec
    }
}


/**
 * @brief ESP-IDF application entry point.
 *
 * Main system startup routine that:
 * - Initializes Wi-Fi and SNTP time.
 * - Starts the HTTP web server and outside-weather fetch task.
 * - Scans the I2C bus for devices and initializes the BME280 sensor.
 * - Reads temperature, pressure, and humidity every ~1 second.
 * - Publishes readings to the web page and evaluates SMS alerts.
 *
 * This function never returns; it runs an infinite loop after initialization.
 *
 * @return None (does not return).
 */
void app_main(void)
{
    
    static volatile bool s_net_ready= 0;    // set true after IP acquired
    static volatile bool s_time_ready= 0;  // set true after SNTP time valid 
    esp_err_t sms; 

    // 0. Bring up Wi-Fi, time, and web server ===
    ESP_ERROR_CHECK(wifi_start_station());    // connect to router (logs GOT_IP)

    // 0.1 Start SNTP (do this once)
    start_sntp_once();

    // 0.1 Start HTTP server at "/"
    web_start();                          

    // 0.2 Start the background task that fetches outside temperature
    xTaskCreate(outside_temp_task, "outside_temp_task", 4096, NULL, 5, NULL);

    // 0.3 Give Wi-Fi/SNTP a moment (tiny, simple polls)
    for (int i = 0; i < 100 && !have_ip();i++) vTaskDelay(pdMS_TO_TICKS(100)); // up to 10s
    for (int i = 0; i < 150 && !time_is_set();i++) vTaskDelay(pdMS_TO_TICKS(150)); // up to 15s
    //set the time & Ip flags 
    s_net_ready  = have_ip();
    s_time_ready = time_is_set();


    // send a quick sms to verify twilo API works 
    /*ESP_LOGI(TAG, "net_ready=%d time_ready=%d", s_net_ready, s_time_ready);
    if (s_net_ready && s_time_ready) {
        ESP_LOGI(TAG, "Sending Twilio self-test…");
        esp_err_t e = sms_send_alert("ESP32 self-test");
        ESP_LOGI(TAG, "Twilio self-test result: %s", esp_err_to_name(e));
    }
    */

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

        //finally, alert the user by sending an sms if needed 
        if(s_time_ready && s_net_ready){
            sms= sms_eval_alert(T_C);  //alert here if temp above or below threshold
            if(sms != ESP_OK){
                ESP_LOGW("ALERT", "sms_eval_alert failed: %s", esp_err_to_name(sms));
            }
        } 
    }
}
