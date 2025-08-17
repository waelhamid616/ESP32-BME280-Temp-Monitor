/*
 * HTTP client (implementation) for Open-Meteo current weather.
 * Handles HTTPS GET with CRT bundle, dynamic buffer read, and minimal JSON scan.
 * Exposes fetch_outside_current(); includes a string-skipping numeric finder.
 * Author: Wael Hamid  |  Date: 2025-08-12
 */


#include "http_client_ext.h"   // Custom wrapper/helpers for HTTP (project specific)
#include "app_config.h"        // Contains config macros like OPEN_METEO_URL
#include "esp_http_client.h"   // ESP-IDF's HTTP client library
#include "esp_crt_bundle.h"    // Built-in SSL/TLS CA certificate bundle (for HTTPS)
#include <stdlib.h>            // malloc, realloc, free, strtod
#include <string.h>            // strstr, strchr, strcmp etc.
#include <math.h>              // NAN, isnan
#include <errno.h>             // errno for error checking with strtod
#include <ctype.h>             // for isspace

/**
 * @brief Search for a numeric value in a JSON-like string by key, skipping strings.
 *
 * Finds the first occurrence of the given key, locates the numeric value after
 * the colon, and parses it as a double. If the value is enclosed in quotes,
 * the search continues to avoid returning strings.
 *
 * @param text Pointer to the JSON/text buffer.
 * @param key  Key string to search for (e.g., "\"temperature_2m\"").
 * @return Parsed number as double on success, NAN if not found or not a number.
 *
 */

static double find_key_number_skip_strings(const char *text, const char *key)
{
    // if no data to search for was passed in, return nan
    if (!text || !key)
    { 
        return NAN;
    }

    const char *search = text;
    const char *ptr;
    const char *new_ptr;
    double v; // stores the data number parsed from json file
    char *endptr = NULL;

    while (1) {
        ptr = strstr(search, key);                 // find the key
        if (!ptr) return NAN;

        ptr = strchr(ptr, ':');                    // find the colon after key
        if (!ptr) return NAN;

        ptr = ptr + 1;                             // move past colon to the value start

        while (*ptr && isspace((unsigned char)*ptr)) ptr++;  // skip spaces

        if (*ptr == '"') {                         // this occurrence is a string → skip it
            search = ptr + 1;                      // advance window so we don't loop on same spot
            continue;
        }

        new_ptr = ptr;                                // start of potential number

        v = strtod(new_ptr, &endptr);                // parse number

        if (endptr != new_ptr )                     // parsed at least one char 
            return v;                              // success

        // Not a number here → advance search window and try next occurrence
        search = ptr + 1;
    }
}


/**
 * @brief Fetch outside temperature and humidity from Open-Meteo API.
 *
 * Performs an HTTPS GET request using the ESP-IDF HTTP client. Reads the
 * response into a dynamically allocated buffer, parses JSON, and extracts
 * temperature_2m (°C) and relative_humidity_2m (%RH).
 *
 * @return weather_t struct with temp and humid fields set, or NAN values on error.
 *
 */

weather_t fetch_outside_current(void)
{
    weather_t out = { NAN, NAN };   // starting clean, safe to return on any error


    double temp,humid;

    // HTTP config client 
    esp_http_client_config_t cfg = {
        .url = OPEN_METEO_URL,                // API endpoint defined in app_config.h
        .timeout_ms = 8000,                  // 8s timeout
        .crt_bundle_attach = esp_crt_bundle_attach, // attach default CA bundle for HTTPS
        .method = HTTP_METHOD_GET,          // GET request
    };

    esp_http_client_handle_t c = esp_http_client_init(&cfg); // create client

    if (!c){ 
    return out; // failed → return NAN                  
        }

    esp_err_t err = esp_http_client_open(c, 0); // open connection, 0 used because we are recieving only 

    // Clean up if a connection cant be opened properly 
    if (err != ESP_OK) 
    { 
        esp_http_client_cleanup(c);
         return out;
        }

    int content_len = esp_http_client_fetch_headers(c); //stores the # of bytes the server sends back, if it doesnt, returns -1. 
    // If server uses "chunked transfer encoding", this might be -1.

    const int CHUNK = 1024; //1KB                     
    int cap=0; 

    // if the number of bytes returned is <64kB
    if (content_len > 0 && content_len < 64*1024){
        cap= content_len + 1; // +1 for null terminator 
    }
    // Otherwise, default to 8 KB buffer
    else{
        cap = CHUNK * 8; 
    }

    char *buf = (char*)malloc(cap); //Allocates a block of memory of size cap bytes on the heap.
    if (!buf) { 
        esp_http_client_close(c);
        esp_http_client_cleanup(c);
        return out; }

    int total = 0;
    while (1) {
        // Ensure buffer has enough room for next chunk
        if (total + CHUNK + 1 > cap) {
            int new_cap = cap * 2;
            char *nb = (char*)realloc(buf, new_cap);

            if (!nb) { free(buf); esp_http_client_close(c);
                 esp_http_client_cleanup(c);
                  return out; }
            buf = nb;
            cap = new_cap;
        }
        int r = esp_http_client_read(c, buf + total, CHUNK);  // read HTTP data
        //returns the number of bytes that got read

        if (r <= 0){
            break;  // 0=end of data, <0=error
        }                                  
        total += r;                                           // accumulate bytes read
    }

    buf[total] = '\0';                                       // null terminate buffer (make it valid string)

    int status = esp_http_client_get_status_code(c);        // get HTTP response status
    if (status == 200 && total > 0) {                      // if OK and data received

        //Fetch outside temperature (Celsius) & Humididty(%RH)
        temp = find_key_number_skip_strings(buf, "\"temperature_2m\"");
        if (!isnan(temp)) out.temp = (float)temp;                  // if valid number, set as output (Non NAN)

        humid= find_key_number_skip_strings(buf, "\"relative_humidity_2m\"");
        if (!isnan(humid)) out.humid = (float)humid;              // if valid number, set as output (NOT NAN)
    
    }

    free(buf);                                        // free buffer
    esp_http_client_close(c);                        // close connection
    esp_http_client_cleanup(c);                     // free client
    return out;                                    // return temperature (or NAN on failure)
}
