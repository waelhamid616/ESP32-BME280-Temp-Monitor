
/*
 * Wi-Fi station (public API).
 * Declares wifi_start_station() which initializes Wi-Fi in STA mode
 * and connects using credentials from app_config.h.
 * Author: Wael Hamid  |  Date: 2025-08-12
 */
#pragma once
#include "esp_err.h"
esp_err_t wifi_start_station(void);


