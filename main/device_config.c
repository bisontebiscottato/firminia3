/*************************************************************
 *                     FIRMINIA 3.2.9                          *
 *  File: device_config.c                                    *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#include <string.h>

#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_err.h"   

#include "device_config.h"


static const char *TAG = "DeviceConfig";

// Global variables definitions
char wifi_ssid[WIFI_SSID_SIZE];
char wifi_password[WIFI_PASSWORD_SIZE];
char web_server[WEB_SERVER_SIZE];
char web_port[WEB_PORT_SIZE];
char web_url[WEB_URL_SIZE];
char api_token[API_TOKEN_SIZE];
char askmesign_user[ASKMESIGN_USER_SIZE];
char api_interval_ms[API_INTERVAL_MS_SIZE];
char language[LANGUAGE_SIZE];

void load_config_from_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "NVS not initialized! Using default values and saving to NVS.");
        // Set default values
        strcpy(wifi_ssid, DEFAULT_WIFI_SSID);
        strcpy(wifi_password, DEFAULT_WIFI_PASSWORD);
        strcpy(web_server, DEFAULT_WEB_SERVER);
        strcpy(web_port, DEFAULT_WEB_PORT);
        strcpy(web_url, DEFAULT_WEB_URL);
        strcpy(api_token, DEFAULT_API_TOKEN);
        strcpy(askmesign_user, DEFAULT_ASKMESIGN_USER);
        strcpy(api_interval_ms, DEFAULT_API_INTERVAL_MS);
        strcpy(language, DEFAULT_LANGUAGE);
        
        // Save default configuration to NVS for future use
        save_config_to_nvs();
        return;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS: %s", esp_err_to_name(err));
        return;
    }

    size_t len;
    
    // Load Wi-Fi SSID
    len = sizeof(wifi_ssid);
    if (nvs_get_str(handle, NVS_WIFI_SSID, wifi_ssid, &len) != ESP_OK || strlen(wifi_ssid) == 0) {
        strcpy(wifi_ssid, DEFAULT_WIFI_SSID);
    }
    
    // Load Wi-Fi Password
    len = sizeof(wifi_password);
    if (nvs_get_str(handle, NVS_WIFI_PASSWORD, wifi_password, &len) != ESP_OK || strlen(wifi_password) == 0) {
        strcpy(wifi_password, DEFAULT_WIFI_PASSWORD);
    }
    
    // Load Web Server
    len = sizeof(web_server);
    if (nvs_get_str(handle, NVS_WEB_SERVER, web_server, &len) != ESP_OK || strlen(web_server) == 0) {
        strcpy(web_server, DEFAULT_WEB_SERVER);
    }
    
    // Load Web Port
    len = sizeof(web_port);
    if (nvs_get_str(handle, NVS_WEB_PORT, web_port, &len) != ESP_OK || strlen(web_port) == 0) {
        strcpy(web_port, DEFAULT_WEB_PORT);
    }
    
    // Load Web URL
    len = sizeof(web_url);
    if (nvs_get_str(handle, NVS_WEB_URL, web_url, &len) != ESP_OK || strlen(web_url) == 0) {
        strcpy(web_url, DEFAULT_WEB_URL);
    }
    
    // Load API Token
    len = sizeof(api_token);
    if (nvs_get_str(handle, NVS_API_TOKEN, api_token, &len) != ESP_OK || strlen(api_token) == 0) {
        strcpy(api_token, DEFAULT_API_TOKEN);
    }
    
    // Load AskMeSign User
    len = sizeof(askmesign_user);
    if (nvs_get_str(handle, NVS_ASKMESIGN_USER, askmesign_user, &len) != ESP_OK || strlen(askmesign_user) == 0) {
        strcpy(askmesign_user, DEFAULT_ASKMESIGN_USER);
    }

    // Load API Interval
    len = sizeof(api_interval_ms);
    if (nvs_get_str(handle, NVS_API_INTERVAL_MS, api_interval_ms, &len) != ESP_OK || strlen(api_interval_ms) == 0) {
        strcpy(api_interval_ms, DEFAULT_API_INTERVAL_MS);
    }

    // Load Language
    len = sizeof(language);
    if (nvs_get_str(handle, NVS_LANGUAGE, language, &len) != ESP_OK || strlen(language) == 0) {
        strcpy(language, DEFAULT_LANGUAGE);
    }
    
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Loaded configuration:");
    ESP_LOGI(TAG, "SSID: %s", wifi_ssid);
    ESP_LOGI(TAG, "Password: %s", (strlen(wifi_password) > 0) ? "******" : "Empty!");
    ESP_LOGI(TAG, "Web Server: %s", web_server);
    ESP_LOGI(TAG, "Web Port: %s", web_port);
    ESP_LOGI(TAG, "Web URL: %s", web_url);
    ESP_LOGI(TAG, "API Token: %s", (strlen(api_token) > 0) ? "******" : "Empty!");
    ESP_LOGI(TAG, "AskMeSign User: %s", askmesign_user);
    ESP_LOGI(TAG, "API Interval check: %s", api_interval_ms);
    ESP_LOGI(TAG, "Language: %s", language);

}

void save_config_to_nvs(void) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS for writing: %s", esp_err_to_name(err));
        return;
    }
    
    nvs_set_str(handle, NVS_WIFI_SSID, wifi_ssid);
    nvs_set_str(handle, NVS_WIFI_PASSWORD, wifi_password);
    nvs_set_str(handle, NVS_WEB_SERVER, web_server);
    nvs_set_str(handle, NVS_WEB_PORT, web_port);
    nvs_set_str(handle, NVS_WEB_URL, web_url);
    nvs_set_str(handle, NVS_API_TOKEN, api_token);
    nvs_set_str(handle, NVS_ASKMESIGN_USER, askmesign_user);
    nvs_set_str(handle, NVS_API_INTERVAL_MS, api_interval_ms);
    nvs_set_str(handle, NVS_LANGUAGE, language);

    nvs_commit(handle);
    nvs_close(handle);
    
    ESP_LOGI(TAG, "Configuration saved successfully to NVS!");
}