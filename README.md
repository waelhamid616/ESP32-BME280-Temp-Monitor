# ESP32 + BME280 (ESP-IDF) Multi-Stage Project

**Author:** Wael Hamid  
**Date:** August 16, 2025  

This ESP-IDF project demonstrates an end-to-end IoT application on the ESP32.  
It integrates a **BME280 environmental sensor** (temperature, pressure, humidity) over I²C with a lightweight **web server** hosted directly on the ESP32. The system compares real-time indoor readings against live outside weather data (via the Open-Meteo API) and presents the results on an auto-refreshing dashboard.  
Future stages extend the design with configurable **alerting features** such as SMS notifications.


---

## Project Stages

**Stage 1 – Sensor Setup & Local Output** (**Completed**)  
- Scan the I²C bus for connected devices  
- Detect and initialize the BME280  
- Read and display temperature, pressure, and humidity once per second over serial  
- Drift-free loop timing using `vTaskDelayUntil`  

**Stage 2 – Local Web Server + Outside Data** (**Completed**)  
- ESP32 connects to Wi-Fi (station mode)  
- Hosts a local HTTP web server (view readings at `/`)  
- Displays inside readings and compares against outside weather (via Open-Meteo API)  
- Auto-refreshing HTML page with temperature & humidity differences  
- Tasks run in parallel:  
  - **Inside sensor (BME280):** updates every ~1 s  
  - **Outside API fetch:** updates every 6 s (adjustable)  
  - **Browser view:** refreshes every 10 s  

**Stage 3 – SMS Alerts** (**Planned**)  
- Integrate with an SMS API  
- Send an alert if inside temperature goes outside configurable thresholds  
- Web interface option to set alert limits


---
## Hardware

- **MCU**: ESP32 (tested with ESP-IDF v5.x)
- **Sensor**: Bosch BME280 (I²C)
- **Wiring (default)**  
  - `SDA -> GPIO 21`  
  - `SCL -> GPIO 22`  
  - Pull-ups: internal pull-ups enabled; external 4.7kΩ recommended
- **I²C Speed**: 100 kHz  
- **Address**: default `0x77` 

---
## File Structure
```
main/
├── app_config.h        # Wi-Fi credentials, API URL, server mode flag
├── app_main.c          # Entry point: Wi-Fi, server, tasks, main loop
├── bme280.c            # BME280 driver implementation (I²C, calibration, compensation)
├── bme280.h            # BME280 driver public API
├── http_client_ext.c   # HTTPS client: fetch outside weather data
├── http_client_ext.h   # Weather struct + client function prototype
├── http_server.c       # Minimal HTTP server, serves HTML dashboard
├── http_server.h       # Web server interface (start + update readings)
├── wifi.c              # Wi-Fi station init and event handlers
├── wifi.h              # Wi-Fi public API
└── CMakeLists.txt      # idf_component_register(...)

```
## Usage
- Create a `.env` file in the project root with your Wi-Fi details:
  ```ini
  WIFI_SSID="YourNetworkName"
  WIFI_PASS="YourPassword"


## Building & Flashing
```bash
idf.py build
idf.py -p COMX flash monitor

