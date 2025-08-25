/*
 * Temperature alert evaluation (public API).
 * - sms_eval_alert(): evaluates temperature against thresholds.
 * - Enforces 30-minute warning and 60-minute alert cooldowns via esp_timer.
 * - Calls sms_send_alert() when a condition is triggered.
 * Author: Wael Hamid  |  Date: 2025-08-18
 */

#pragma once
#include "esp_err.h"
esp_err_t sms_eval_alert(double T_C);