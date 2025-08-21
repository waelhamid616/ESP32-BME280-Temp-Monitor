// sms_client.c
#include "sms_client.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "certs.h"
#include <stdio.h>
#include <string.h>

static const char *TAG= "sms";

esp_err_t sms_send_alert(const char *body){
    char url[200];
    snprintf(url,sizeof url, "https://api.twilio.com/2010-04-01/Accounts/%s/Messages.json",
    CONFIG_TWILIO_ACCOUNT_SID); // saves the twilo url into "url"
    // retuns the number of characters saved.

    // URL-encode the message body
    char body_enc[256];
    //body_enc is the destination buffer the encoded meassage will be stored in
    url_encode(body, body_enc, sizeof body_enc);

    // Build x-www-form-url encoded form body
    char form[512];
    // n is length of the final HTTP body string (form), used as Content-Length.
    int n = snprintf(form, sizeof form,
                 "To=%s&From=%s&Body=%s",
                 CONFIG_ALERT_TO_NUMBER,
                 CONFIG_TWILIO_FROM_NUMBER,
                 body_enc);


    // cleint init
    esp_http_client_config_t cfg = {
        .url= url,
        .crt_bundle_attach= esp_crt_bundle_attach,
        .timeout_ms= 10000,
    };

    //crate the client handle 
    esp_http_client_handle_t h = esp_http_client_init(&cfg);
    if (!h) return ESP_FAIL;

    //set request parameters 
    //The data I’m sending in the body is in form-encoded format (like HTML forms).
    esp_http_client_set_header(h, "Content-Type", "application/x-www-form-urlencoded");
    esp_http_client_set_post_field(h, form, n); //everything needed here 

    //perfrom the client request 
    esp_err_t err = esp_http_client_perform(h);

    // If the HTTP client failed to perform the request (network error, TLS error, etc.)
    if (err != ESP_OK) { 
        ESP_LOGE(TAG, "HTTP perform: %s", esp_err_to_name(err)); // Log the error string name (e.g. "ESP_ERR_HTTP_CONNECT")

         goto out; //jump to out directly and skip the rest 
         }

    // Get the HTTP response status code (e.g. 200, 201, 400, 500)
    int status = esp_http_client_get_status_code(h); 

    // If the status is NOT 2xx (e.g., 200 OK or 201 Created),
    // then the request failed from Twilio’s perspective.
    if (status / 100 != 2) {
        //read the responce back, usually sent in json 
        char buf[256]; // 256 bytes 
        int r = esp_http_client_read_response(h, buf, sizeof buf - 1);

        if (r>0){ //if something was read back 
            buf[r]=0; ESP_LOGE(TAG, "Twilio %d: %s", status, buf); // Add a null terminator so buf is a proper C string.
         }
        else    { //nothing was read back 
             ESP_LOGE(TAG, "Twilio %d (no body)", status); }
        err = ESP_FAIL;
    } else {
        // success (201 Created expected)
        ESP_LOGI(TAG, "Twilio OK: %d", status);
    }
out:
    //perform cleanup 
    esp_http_client_cleanup(h);
    return err;
}