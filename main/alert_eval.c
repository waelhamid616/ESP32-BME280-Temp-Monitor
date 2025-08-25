/*
 * Temperature alert evaluation (implementation).
 * - Evaluates °C readings against warn/alert thresholds.
 * - Enforces cooldowns with esp_timer one-shot timers (30m warn, 60m alert).
 * - Sends SMS via sms_send_alert() when conditions are met.
 * Author: Wael Hamid  |  Date: 2025-08-18
 */

#include "alert_eval.h"
#include <stdio.h>
#include <stdint.h>
#include "esp_err.h"
#include "sdkconfig.h"
#include <stdbool.h> 
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// -----------------------------------------------------------------------------
// Duration helpers
// esp_timer APIs accept time values in microseconds (µs). Use 64-bit constants
// (ULL) to avoid overflow during compile-time arithmetic.
// -----------------------------------------------------------------------------
#define THIRTY_MIN_US  (30ULL * 60ULL * 1000000ULL)   // 30 min (1,800,000,000 µs)
#define ONE_HOUR_US    (60ULL * 60ULL * 1000000ULL)   // 60 min (3,600,000,000 µs)

// -----------------------------------------------------------------------------
// Cooldown flags
// 0 = alert allowed (no cooldown active)
// 1 = alert suppressed (cooldown active)
// A: 30-minute cooldown (warnings)
// B: 60-minute cooldown (alerts)
// -----------------------------------------------------------------------------
static volatile bool a_tick = 0;
static volatile bool b_tick = 0;

// Timer handles are opaque references returned by esp_timer_create() and
// required to start/stop/delete timers.
static esp_timer_handle_t a_handle = NULL;  // 30-minute cooldown timer
static esp_timer_handle_t b_handle = NULL;  // 60-minute cooldown timer

/**
 * @brief Cooldown expiry for warnings (30 minutes).
 *
 * Timer A callback: clears the warning cooldown flag to re-enable warnings.
 *
 * @param arg Unused.
 */

static void callback_timer_A(void *arg) { a_tick = 0; }  // Re-enable warnings

/**
 * @brief Cooldown expiry for alerts (60 minutes).
 *
 * Timer B callback: clears the alert cooldown flag to re-enable alerts.
 *
 * @param arg Unused.
 */
static void callback_timer_B(void *arg) { b_tick = 0; }  // Re-enable alerts

/**
 * @brief Create one-shot cooldown timers.
 *
 * Initializes Timer A (30m) and Timer B (60m) with task-dispatched callbacks.
 * Safe to call once before first use.
 */
static void timers_init(void) {
    const esp_timer_create_args_t a_args = {
        .callback = &callback_timer_A,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,  // Task context: safer API surface
        .name = "TimerA_30min"
    };
    const esp_timer_create_args_t b_args = {
        .callback = &callback_timer_B,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "TimerB_60min"
    };

    ESP_ERROR_CHECK(esp_timer_create(&a_args, &a_handle));
    ESP_ERROR_CHECK(esp_timer_create(&b_args, &b_handle));
}

// Application-specific stub: send an SMS. Replace with your real function.
extern esp_err_t sms_send_alert(const char *msg);

// Thresholds 
#define ALERT_LOW_C   15.0
#define WARN_LOW_C    16.5
#define WARN_HIGH_C   28.5
#define ALERT_HIGH_C  30.0

/**
 * @brief Evaluate temperature and send SMS subject to cooldowns.
 *
 * Sends warnings when Cold < T_C <= Cold_Warn or Hot_Warn <= T_C < Hot
 * (30-minute cooldown). Sends alerts when T_C <= Cold or T_C >= Hot
 * (60-minute cooldown). Starts the appropriate one-shot timer after send.
 *
 * @param[in] T_C Temperature in degrees Celsius.
 *
 * @return ESP_OK if no send was needed or after a successful send;
 *         error code from sms_send_alert() or timer APIs on failure.
 */
esp_err_t sms_eval_alert(double T_C) {

    // Ensure timers are created before first use.
    if (a_handle == NULL || b_handle == NULL) {
        timers_init();
    }

    const double Cold = ALERT_LOW_C ;           // Low temp alert at 15.0 deg
    const double Cold_Warn  = WARN_LOW_C ;     // Low temp warning 16.5 deg
    const double Hot_Warn = WARN_HIGH_C;      // High Temp Warning 28.5
    const double Hot  = ALERT_HIGH_C;        // High Temp Threshold 30.0 

    char msg[120];

    // Cold warning: (Cold < T_C <= Cold_Warn) AND not on 30-min cooldown
    if ((!a_tick) && (T_C > Cold) && (T_C <= Cold_Warn)) {
        snprintf(msg, sizeof msg, "Cold Warning: Inside temperature %.1fC is below %.1fC.", T_C, Cold_Warn);
        a_tick = 1;  // Enter 30-minute cooldown for warnings
        ESP_ERROR_CHECK(esp_timer_start_once(a_handle, THIRTY_MIN_US));
        return sms_send_alert(msg);
    }

    // Cold alert: (T_C <= Cold) AND not on 60-min cooldown
    if ((!b_tick) && (T_C <= Cold)) {
        snprintf(msg, sizeof msg, "Cold Alert: Inside temperature %.1fC is below %.1fC.", T_C, Cold);
        b_tick = 1;  // Enter 60-minute cooldown for alerts
        ESP_ERROR_CHECK(esp_timer_start_once(b_handle, ONE_HOUR_US));
        return sms_send_alert(msg);
    }

    // Hot warning: (Hot_Warn <= T_C < Hot) AND not on 30-min cooldown
    if ((!a_tick) && (T_C >= Hot_Warn) && (T_C < Hot)) {
        snprintf(msg, sizeof msg, "Hot Warning: Inside temperature %.1fC is above %.1fC.", T_C, Hot_Warn);
        a_tick = 1;  // Enter 30-minute cooldown for warnings
        ESP_ERROR_CHECK(esp_timer_start_once(a_handle, THIRTY_MIN_US));
        return sms_send_alert(msg);
    }

    // Hot alert: (T_C >= Hot) AND not on 60-min cooldown
    if ((!b_tick) && (T_C >= Hot)) {
        snprintf(msg, sizeof msg, "Hot Alert: Inside temperature %.1fC is above %.1fC.", T_C, Hot);
        b_tick = 1;  // Enter 60-minute cooldown for alerts
        ESP_ERROR_CHECK(esp_timer_start_once(b_handle, ONE_HOUR_US));
        return sms_send_alert(msg);
    }

    // In-range or suppressed by cooldown: nothing to do.
    return ESP_OK;
}







