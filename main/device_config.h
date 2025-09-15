/*************************************************************
 *                     FIRMINIA 3.0                          *
 *  File: device_config.h                                    *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#ifndef DEVICE_CONFIG_H
#define DEVICE_CONFIG_H

#include <stdbool.h>

// NVS keys definitions
#define NVS_NAMESPACE         "config"
#define NVS_WIFI_SSID         "wifi_ssid"
#define NVS_WIFI_PASSWORD     "wifi_password"
#define NVS_WEB_SERVER        "web_server"
#define NVS_WEB_PORT          "web_port"
#define NVS_WEB_URL           "web_url"
#define NVS_API_TOKEN         "api_token"
#define NVS_ASKMESIGN_USER    "askmesign_user"
#define NVS_API_INTERVAL_MS   "api_interval_ms"
#define NVS_LANGUAGE          "language"

// Buffer sizes for string parameters
#define WIFI_SSID_SIZE        33
#define WIFI_PASSWORD_SIZE    65
#define WEB_SERVER_SIZE       64
#define WEB_PORT_SIZE         6
#define WEB_URL_SIZE          256
#define API_TOKEN_SIZE        64
#define ASKMESIGN_USER_SIZE   64
#define API_INTERVAL_MS_SIZE  12
#define LANGUAGE_SIZE          2

// Global configuration variables
extern char wifi_ssid[WIFI_SSID_SIZE];
extern char wifi_password[WIFI_PASSWORD_SIZE];
extern char web_server[WEB_SERVER_SIZE];
extern char web_port[WEB_PORT_SIZE];
extern char web_url[WEB_URL_SIZE];
extern char api_token[API_TOKEN_SIZE];
extern char askmesign_user[ASKMESIGN_USER_SIZE];
extern char api_interval_ms[API_INTERVAL_MS_SIZE];
extern char language[LANGUAGE_SIZE];

// Default values - Non-functional placeholders that require BLE configuration
#define DEFAULT_WIFI_SSID        ""
#define DEFAULT_WIFI_PASSWORD    ""
#define DEFAULT_WEB_SERVER       "sign.askme.it"
#define DEFAULT_WEB_PORT         "443"
#define DEFAULT_WEB_URL          "https://sign.askme.it/api/v2/files/pending?page=0&size=1"
#define DEFAULT_API_TOKEN        ""
#define DEFAULT_ASKMESIGN_USER   ""
#define DEFAULT_API_INTERVAL_MS  "30000"
#define DEFAULT_LANGUAGE         "0"

// Function declarations
void load_config_from_nvs(void);
void save_config_to_nvs(void);
bool is_config_default(void);
void reset_config_to_default(void);

#endif // DEVICE_CONFIG_H
