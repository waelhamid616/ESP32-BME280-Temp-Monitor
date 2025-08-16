#ifndef HTTP_CLIENT_EXT_H
#define HTTP_CLIENT_EXT_H

#include <math.h>

typedef struct {
    float temp;   // °C
    float humid;  // %RH
} weather_t;

weather_t fetch_outside_current(void);
//static const char* skip_ws(const char* p);
#endif
