/*
 * Project configuration header.
 * Wi-Fi credentials, Open-Meteo API URL for Vancouver (temp & RH), and server mode.
 * Included by modules that need network and server configuration.
 * Author: Wael Hamid  |  Date: 2025-08-12
 */

#pragma once

// ==== Wi-Fi credentials ====
#ifndef CONFIG_WIFI_SSID
#define CONFIG_WIFI_SSID ""
#endif
#ifndef CONFIG_WIFI_PASS
#define CONFIG_WIFI_PASS ""
#endif

#define WIFI_SSID CONFIG_WIFI_SSID
#define WIFI_PASS CONFIG_WIFI_PASS

// ==== Outside temperature API ====
#define OPEN_METEO_URL "https://api.open-meteo.com/v1/forecast?latitude=49.2827&longitude=-123.1207&current=temperature_2m,relative_humidity_2m"

// ==== Server mode ====
#define USE_HTTPS_SERVER 0   // 0 = use HTTP server, 1 = use HTTPS server
