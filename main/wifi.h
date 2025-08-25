
/*
 * Wi-Fi station (public API).
 * - wifi_start_station(): initialize and start STA mode with credentials from app_config.h.
 * - Utility helpers (inline in .c): check for IPv4, check system time, and start SNTP.
 * Author: Wael Hamid  |  Date: 2025-08-12
 */

#pragma once
#include "esp_err.h"
esp_err_t wifi_start_station(void);


