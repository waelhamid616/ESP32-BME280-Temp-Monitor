# --- Load Wi‑Fi creds from .env (at project root) ---
set(DOTENV_PATH "${CMAKE_SOURCE_DIR}/.env")

if(EXISTS "${DOTENV_PATH}")
    file(STRINGS "${DOTENV_PATH}" ENV_LINES)
    foreach(line IN LISTS ENV_LINES)
        if(line MATCHES "^WIFI_SSID=")
            string(REPLACE "WIFI_SSID=" "" WIFI_SSID "${line}")
        elseif(line MATCHES "^WIFI_PASS=")
            string(REPLACE "WIFI_PASS=" "" WIFI_PASS "${line}")
        endif()
    endforeach()
endif()

# Defaults if not set
if(NOT DEFINED WIFI_SSID)
    set(WIFI_SSID "")
endif()
if(NOT DEFINED WIFI_PASS)
    set(WIFI_PASS "")
endif()

idf_component_register(
  SRCS
    "app_main.c"
    "wifi.c"
    "http_server.c"
    "http_client_ext.c"
    "bme280.c"
    "sms_client.c"
    "alert_eval.c"
  INCLUDE_DIRS
    "."
  REQUIRES
    driver
    esp_wifi
    esp_event
    esp_netif
    nvs_flash
    esp_http_server
    esp_http_client
    esp-tls
    esp_https_server
    esp_timer
)

# Pass values into code (visible as preprocessor macros)
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    CONFIG_WIFI_SSID="${WIFI_SSID}"
    CONFIG_WIFI_PASS="${WIFI_PASS}"
)

# Show SSID at configure time (but not the password)
message(STATUS "WIFI_SSID='${WIFI_SSID}' (from .env)")
