#include "http_server.h"         // our header: web_start(), web_set_readings()
#include "app_config.h"          // USE_HTTPS_SERVER flag
#include "esp_http_server.h"     // HTTP server API (httpd_start, handlers)
#include "esp_log.h"             // ESP_LOGI
#include <math.h>                // NAN, isnan


static const char *TAG = "http_server";

// Latest readings shown on the page.
// Set from app code via web_set_readings().
static float t_in  = NAN;         // inside temperature (°C)
static float t_out = NAN;        // outside temperature (°C)
static float h_in  = NAN;       // inside humidity (%RH)
static float h_out = NAN;      // outside humidity (%RH)

// Public: update the values that the page will display.
// Call this whenever you have new sensor/API readings.
void web_set_readings(float in_c, float out_c,float in_h, float out_h) {
    t_in = in_c;
    t_out = out_c;
    h_in = in_h;
    h_out = out_h;
     }

// HTTP handler for GET "/"
// Builds a tiny HTML string and sends it as the response body.
static esp_err_t root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");

    char  buf[512];
    float diff = (isnan(t_in) || isnan(t_out)) ? NAN : (t_in - t_out);

    // Simple auto-refreshing page (every 10s) with 3 values. 
    int n = snprintf(buf, sizeof(buf),
        "<!doctype html><meta charset=utf-8>"
        "<meta http-equiv=refresh content=10>"
        "<h1>ESP32 Weather Monitor</h1>"
        "<p>Inside: %.2f &deg;C</p>"
        "<p>Outside: %.2f &deg;C</p>"
        "<p>Diff: %s</p>",
        t_in, t_out,
        isnan(diff) ? "N/A" : ({ static char d[32]; snprintf(d, sizeof d, "%.2f &deg;C", diff), d; })
    );

    return httpd_resp_send(req, buf, n);   // send the page
}

// Start a plain HTTP server on port 80 and register the "/" route.
static httpd_handle_t start_http(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();  // sensible defaults
    httpd_handle_t s = NULL;

    if (httpd_start(&s, &cfg) == ESP_OK) {
        httpd_uri_t root = {
            .uri     = "/",
            .method  = HTTP_GET,
            .handler = root_get, // giving the esp idf the address of the function for when a request comes in 
            .user_ctx = NULL
        };
        httpd_register_uri_handler(s, &root);
    }
    return s;  // (unused, but returned in case server is stopped later)
}

// Public: start the web server (HTTP or HTTPS based on USE_HTTPS_SERVER).
// Call this once AFTER Wi-Fi has connected (GOT_IP).
void web_start(void) {
    start_http();
    ESP_LOGI(TAG, "Web server started");
}
