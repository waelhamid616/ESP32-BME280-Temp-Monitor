/*
 * HTTP client (public API) for outside weather fetch.
 * Defines weather_t {temp, humid} and fetch_outside_current() using Open-Meteo.
 * Consumers include app_main task that updates the web page.
 * Author: Wael Hamid  |  Date: 2025-08-12
 */

#ifndef HTTP_CLIENT_EXT_H
#define HTTP_CLIENT_EXT_H

#include <math.h>

typedef struct {
    float temp;   // °C
    float humid;  // %RH
} weather_t;

weather_t fetch_outside_current(void);
#endif
