# ESP32 + BME280 (ESP-IDF) Multi-Stage Project

**Author:** Wael Hamid  
**Date:** August 9, 2025  

This is an ESP-IDF project for reading **temperature, pressure, and humidity** from a **BME280** sensor over I²C, and then progressively adding network and alert features.

---
## Project Stages

**Stage 1 – Sensor Setup & Local Output** (**Completed**)
- Scan the I²C bus for connected devices  
- Detect and initialize the BME280  
- Read and display temperature, pressure, and humidity once per second over serial  
- Drift-free loop timing using `vTaskDelayUntil`  
- **Stage 1 Output Example**
- Found Sensor at 0x77
- T=27.44 °C  P=1008.92 hPa  H=42.2 %RH

**Stage 2 – Data Upload & Comparison (Planned)**  
- Host a small web server on the ESP32 over HTTPS  
- Upload sensor readings to the server  
- Pull outside temperature data from a public weather API  
- Compare inside (BME280) vs. outside temperature in real-time  

**Stage 3 – SMS Alerts (Planned)**  
- Integrate with an SMS API 
- Send an SMS alert if temperature rises above **30°C** or falls below **15°C**  
- Allow configurable thresholds via web interface or configuration file  

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
├── bme280.c        # BME280 driver implementation
├── bme280.h        # BME280 driver public API
├── app_main.c      # Stage 1 main: scan, init, read, print
└── CMakeLists.txt  # idf_component_register(...)
```


## Building & Flashing
```bash
idf.py build
idf.py -p COMX flash monitor

