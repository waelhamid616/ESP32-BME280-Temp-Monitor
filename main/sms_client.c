// sms_client.c
#include "sms_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"   // Use the Mozilla root CA bundle (no site-specific cert needed)
#include "esp_log.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "sms";

// url_encode() must percent-encode reserved characters for application/x-www-form-urlencoded.
// You already have/plan this helper elsewhere; we just declare it here.
extern void url_encode(const char *in, char *out, size_t out_sz);

esp_err_t sms_send_alert(const char *body) {
    // ------------------------------------------------------------------------
    // Build the Twilio Messages API endpoint:
    //   https://api.twilio.com/2010-04-01/Accounts/{AccountSid}/Messages.json
    // We inject the Account SID from Kconfig.
    // ------------------------------------------------------------------------
    char url[200];
    snprintf(url, sizeof url,
             "https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json",
             CONFIG_TWILIO_ACCOUNT_SID);  // Saves the full Twilio URL into `url`

    // ------------------------------------------------------------------------
    // URL-encode all form fields for "application/x-www-form-urlencoded".
    // IMPORTANT:
    //   - In form-encoding, '+' means space, so phone numbers like +1604555...
    //     must be encoded to %2B (your url_encode should do this).
    //   - We encode To, From, and Body to be safe.
    // ------------------------------------------------------------------------
    char to_enc[64], from_enc[64], body_enc[256];
    url_encode(CONFIG_ALERT_TO_NUMBER,     to_enc,   sizeof to_enc);    // destination number
    url_encode(CONFIG_TWILIO_FROM_NUMBER,  from_enc, sizeof from_enc);  // your Twilio number (sender)
    url_encode(body,                        body_enc, sizeof body_enc);  // SMS text payload

    // ------------------------------------------------------------------------
    // Build the POST body in classic HTML form format:
    //   To=...&From=...&Body=...
    // `n` is the final length, used by the HTTP client as Content-Length.
    // ------------------------------------------------------------------------
    char form[512];
    int n = snprintf(form, sizeof form,
                     "To=%s&From=%s&Body=%s",
                     to_enc, from_enc, body_enc);

    // ------------------------------------------------------------------------
    // HTTP client configuration:
    //   - POST method
    //   - Basic Auth (username = Account SID, password = Auth Token)
    //   - TLS trust via the built-in certificate bundle (no per-site cert)
    //   - Reasonable timeout
    // ------------------------------------------------------------------------
    esp_http_client_config_t cfg = {
        .url = url,
        .method = HTTP_METHOD_POST,                 // Explicitly do a POST
        .auth_type = HTTP_AUTH_TYPE_BASIC,          // Twilio uses HTTP Basic Auth
        .username = CONFIG_TWILIO_ACCOUNT_SID,      // Username = Account SID
        .password = CONFIG_TWILIO_AUTH_TOKEN,       // Password = Auth Token
        .crt_bundle_attach = esp_crt_bundle_attach, // Use Mozilla CA bundle
        .timeout_ms = 10000,                        // 10-second network timeout
    };

    // Create the HTTP client handle (opaque object that holds connection state)
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) return ESP_FAIL;

    // ------------------------------------------------------------------------
    // Set request headers and body:
    //   - Content-Type informs Twilio that we're sending form-encoded fields.
    //   - Post field sets the body pointer + length (Content-Length is derived).
    // ------------------------------------------------------------------------
    esp_http_client_set_header(h, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(h, form, n);  // Everything needed is in `form`

    // ------------------------------------------------------------------------
    // Perform the HTTP request:
    //   - Handles DNS, TCP, TLS, Basic Auth handshake, send, and receive.
    //   - Returns ESP_OK only if transport and TLS succeeded and we got a
    //     valid HTTP response from the server.
    // ------------------------------------------------------------------------
    esp_err_t err = esp_http_client_perform(h);

    // If transport/TLS failed (network error, handshake problem, etc.), log and bail.
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP perform: %s", esp_err_to_name(err));
        esp_http_client_cleanup(h);
        return err;
    }

    // ------------------------------------------------------------------------
    // Check HTTP status code:
    //   - Twilio returns 201 Created on success for /Messages.json
    //   - Any non-2xx code indicates an API-side error (e.g., auth, params, etc.)
    // ------------------------------------------------------------------------
    int status = esp_http_client_get_status_code(h);
    if (status / 100 != 2) {
        // Read response body (usually JSON error with `message`/`code`)
        char buf[256];
        int r = esp_http_client_read_response(h, buf, sizeof buf - 1);

        if (r > 0) {
             buf[r] = 0; ESP_LOGE(TAG, "Twilio %d: %s", status, buf);
         }

        else        {             
            ESP_LOGE(TAG, "Twilio %d (no body)", status);
         }

        err = ESP_FAIL;
        
    } else {
        ESP_LOGI(TAG, "Twilio OK: %d", status);  // Typically 201
    }

    // Always cleanup the client handle to free resources/sockets
    esp_http_client_cleanup(h);
    return err;
}
