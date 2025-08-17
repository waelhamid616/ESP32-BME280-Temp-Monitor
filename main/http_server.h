/*
 * Minimal HTTP server interface.
 * web_start() launches the server; web_set_readings() updates values shown on "/".
 * Intended to be called after Wi-Fi connects (GOT_IP).
 * Author: Wael Hamid  |  Date: 2025-08-12
 */

#pragma once
void web_start(void);
void web_set_readings(float in_c, float out_c,float in_h, float out_h);