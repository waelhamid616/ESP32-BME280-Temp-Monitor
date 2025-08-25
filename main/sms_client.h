/*
 * SMS client (public API).
 * - sms_send_alert(): send SMS via Twilio REST API (TLS + Basic Auth).
 * - url_encode(): helper for application/x-www-form-urlencoded fields.
 * Author: Wael Hamid  |  Date: 2025-08-20
 */

#pragma once
#include "esp_err.h"
esp_err_t sms_send_alert(const char *body);
// url_encode() must percent-encode reserved characters for application/x-www-form-urlencoded.
static int url_encode(const char *in, char *out, int outlen) ;
