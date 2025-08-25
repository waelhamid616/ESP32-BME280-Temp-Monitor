/*
 * Wi-Fi station bring-up and utilities (implementation).
 * - Initializes NVS/esp_netif/event loop; configures and starts STA mode.
 * - Registers event handlers for connect/retry and logs acquired IPv4.
 * - Utility checks: have_ip() and time_is_set() to gate network/TLS.
 * - start_sntp_once(): one-shot SNTP bootstrap using time.google.com.
 * Author: Wael Hamid  |  Date: 2025-08-12
 */

#include "wifi.h"
#include "app_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include <string.h>           
#include "sdkconfig.h"


static const char *TAG = "wifi";

/**
 * @brief Event handler for Wi-Fi and IP events.
 *
 * - On STA_START: triggers Wi-Fi connection attempt.
 * - On STA_DISCONNECTED: logs event and retries connection.
 * - On GOT_IP: logs the acquired IP address.
 *
 */
static void handler(void *arg, esp_event_base_t base, int32_t id, void *data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "Disconnected; reconnecting...");
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = (ip_event_got_ip_t*)data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&e->ip_info.ip));
    }
}

/**
 * @brief Initialize and start Wi-Fi in station mode.
 *
 * Initializes NVS, network interfaces, and event loop. Creates default
 * Wi-Fi station, registers event handlers, configures credentials,
 * and starts the Wi-Fi driver in STA mode.
 *
 * @return ESP_OK on success, or an error code on failure.
 *
 */
esp_err_t wifi_start_station(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler, NULL));

    wifi_config_t wc = { 0 };
    //copy the ssid and pass into wc 
    strlcpy((char*)wc.sta.ssid, CONFIG_WIFI_SSID, sizeof(wc.sta.ssid));
    strlcpy((char*)wc.sta.password, CONFIG_WIFI_PASSWORD, sizeof(wc.sta.password));

    ESP_LOGI(TAG, "Using SSID:'%s' (len=%u)", WIFI_SSID, (unsigned)strlen(WIFI_SSID));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));  
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wc));
    ESP_ERROR_CHECK(esp_wifi_start());
    return ESP_OK;
}

/**
 * @brief Check if Wi-Fi station has an IPv4.
 *
 * Looks up "WIFI_STA_DEF" netif, queries IP, and returns true if
 * DHCP assigned a non-zero IPv4 address.
 *
 * @return true if station has valid IPv4, false otherwise.
 */

static inline bool have_ip(void) {                                       // true if Wi-Fi STA has an IPv4
    esp_netif_t *sta = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"); // default Wi-Fi station netif
    if (!sta) return false;                                            // netif not created/available yet
    esp_netif_ip_info_t ip= {0};                                      // holder for IPv4 info
    if (esp_netif_get_ip_info(sta, &ip) != ESP_OK) return false;     // no ip yet...
    return ip.ip.addr != 0;                                         // true when DHCP gave us an IP
}

/**
 * @brief Check if system time is valid.
 *
 * Reads epoch seconds and considers time valid if greater than
 * 1700000000 (~Nov 2023). Used to gate TLS, logging, and
 * timestamped operations.
 *
 * @return true if system time is valid, false otherwise.
 */

static inline bool time_is_set(void) {      // true if system clock looks valid
    time_t now = 0;                        // initialize to epoch
    time(&now);                           // now holds the current epoch seconds after call 
    return now > 1700000000;             // > ~Nov 2023 ⇒ treat as “real” time
}

/**
 * @brief Start SNTP client once.
 *
 * Enables SNTP in poll mode with "time.google.com" as the server,
 * and initializes the SNTP service if not already running.
 */
static inline void start_sntp_once(void) {
    if (!esp_sntp_enabled()) {
        esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
        esp_sntp_setservername(0, "time.google.com");   
        esp_sntp_init();
    }
}
