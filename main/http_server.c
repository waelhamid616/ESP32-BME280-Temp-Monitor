/*
 * Minimal HTTP server (implementation).
 * Serves a compact HTML dashboard with inside/outside T/H and deltas.
 * Uses module-local storage updated via web_set_readings().
 * Author: Wael Hamid  |  Date: 2025-08-12
 */


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

/**
 * @brief Update the stored sensor values for web display.
 *
 * Stores inside/outside temperature and humidity in static variables
 * so the HTTP handler can display them on the served web page.
 *
 */

void web_set_readings(float in_c, float out_c,float in_h, float out_h) {
    t_in = in_c;
    t_out = out_c;
    h_in = in_h;
    h_out = out_h;
     }

/**
 * @brief HTTP handler for GET "/".
 *
 * Generates and returns an HTML page showing inside/outside temperature,
 * humidity, their differences, and a note about recommended ranges.
 * Auto-refreshes every 10 seconds using a meta tag.
 *
 * @return ESP_OK on success, or an error code on failure.
 *
 */

static esp_err_t root_get(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html"); //html style here...

    // declare a 4kB nuffer to store the html string 
        char  buf[1024];
    //calculate the outside vs inside temperature and humididty difference
    //if either outside or insdie temp is not a num -> set to NAN, otherwise, calculate the difference 
    float t_diff = (isnan(t_in) || isnan(t_out)) ? NAN : (fabs(t_in - t_out));  
    float h_diff = (isnan(t_in) || isnan(t_out)) ? NAN : (fabs(h_in - h_out));

    // simple safety rule: inside temp 15–30°C, inside RH 30–60%
    bool temp_ok  = (!isnan(t_in) && t_in >= 15.0f && t_in <= 30.0f);
    bool humid_ok = (!isnan(h_in) && h_in >= 30.0f && h_in <= 60.0f);

    const char *note = (temp_ok && humid_ok)
        ? "Inside conditions are within the recommended range (15\u201330\u00B0C, 30\u201360% RH)."
        : "Inside conditions are outside the recommended range (15\u201330\u00B0C, 30\u201360% RH).";
    // Compact HTML: small CSS + simple table
    int n = snprintf(buf, sizeof(buf),
    "<!doctype html><meta charset=utf-8>"
    "<meta name=viewport content='width=device-width,initial-scale=1'>"
    "<meta http-equiv=refresh content=10>"
    "<title>ESP32 Weather Monitor</title>"
    "<style>"
      "body{font-family:sans-serif;margin:20px;background:#fafafa}"
      "h1{margin:0 0 12px;font-size:20px}"
      ".row{display:flex;justify-content:flex-start;gap:6px;...}"
      "hr{border:none;border-top:1px solid #ccc;margin:8px 0}"
      ".note{margin-top:10px;font-size:14px;color:#444}"
    "</style>"
    "<h1>ESP32 Smart Climate Monitor</h1>"
    "<div class=row><b>Inside Temp:</b><span>%.2f &deg;C</span></div>"
    "<div class=row><b>Outside Temp:</b><span>%.2f &deg;C</span></div>"
    "<div class=row><b>Temp &Delta;:</b><span>%.2f &deg;C</span></div>"
    "<hr>"
    "<div class=row><b>Inside Humidity:</b><span>%.0f %%RH</span></div>"
    "<div class=row><b>Outside Humidity:</b><span>%.0f %%RH</span></div>"
    "<div class=row><b>Humidity &Delta;:</b><span>%.2f %%RH</span></div>"
    "<p class=note>%s</p>",
    t_in, t_out, t_diff, h_in, h_out, h_diff, note
    );

    if (n < 0) n = 0;
    if (n >= (int)sizeof(buf)) n = sizeof(buf) - 1;  // safety clamp

    return httpd_resp_send(req, buf, n); // return the page...
}

/**
 * @brief Start the HTTP server and register the root handler.
 *
 * Initializes and launches the ESP-IDF HTTP server with default settings,
 * then registers the "/" URI handler for GET requests.
 *
 * @return Handle to the HTTP server instance, or NULL on failure.
 *
 */
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

/**
 * @brief Public entry point to start the web server.
 *
 * Starts the HTTP server after Wi-Fi has connected. Logs a message
 * confirming the server startup.
 *
 */

void web_start(void) {
    start_http();
    ESP_LOGI(TAG, "Web server started");
}
