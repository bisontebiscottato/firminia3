#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

// Definizione delle variabili globali
char wifi_ssid[32];
char wifi_password[64];
char web_server[64];
char web_port[6];
char web_url[128];
char api_token[64];
char askmesign_user[64];

static const char *TAG = "Config";

/**
 * Carica i parametri salvati in NVS
 */
void load_config_from_nvs() {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "⚠️ NVS non inizializzato! Uso valori di default e salvo in NVS.");
        
        // Assegna i valori di default
        strcpy(wifi_ssid, DEFAULT_WIFI_SSID);
        strcpy(wifi_password, DEFAULT_WIFI_PASSWORD);
        strcpy(web_server, DEFAULT_WEB_SERVER);
        strcpy(web_port, DEFAULT_WEB_PORT);
        strcpy(web_url, DEFAULT_WEB_URL);
        strcpy(api_token, DEFAULT_API_TOKEN);
        strcpy(askmesign_user, DEFAULT_ASKMESIGN_USER);

        // Salva i valori in NVS per la prossima volta
        save_config_to_nvs(wifi_ssid, wifi_password, web_server, web_port, web_url, api_token, askmesign_user);
        
        return;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "Errore nell'apertura di NVS: %s", esp_err_to_name(err));
        return;
    }

    size_t len;

    // Caricamento e controllo Wi-Fi SSID
    len = sizeof(wifi_ssid);
    if (nvs_get_str(handle, NVS_WIFI_SSID, wifi_ssid, &len) != ESP_OK || strlen(wifi_ssid) == 0) {
        strcpy(wifi_ssid, DEFAULT_WIFI_SSID);
    }

    // Caricamento e controllo Wi-Fi Password
    len = sizeof(wifi_password);
    if (nvs_get_str(handle, NVS_WIFI_PASSWORD, wifi_password, &len) != ESP_OK || strlen(wifi_password) == 0) {
        strcpy(wifi_password, DEFAULT_WIFI_PASSWORD);
    }

    // Caricamento e controllo Web Server
    len = sizeof(web_server);
    if (nvs_get_str(handle, NVS_WEB_SERVER, web_server, &len) != ESP_OK || strlen(web_server) == 0) {
        strcpy(web_server, DEFAULT_WEB_SERVER);
    }

    // Caricamento e controllo Web Port
    len = sizeof(web_port);
    if (nvs_get_str(handle, NVS_WEB_PORT, web_port, &len) != ESP_OK || strlen(web_port) == 0) {
        strcpy(web_port, DEFAULT_WEB_PORT);
    }

    // Caricamento e controllo Web URL
    len = sizeof(web_url);
    if (nvs_get_str(handle, NVS_WEB_URL, web_url, &len) != ESP_OK || strlen(web_url) == 0) {
        strcpy(web_url, DEFAULT_WEB_URL);
    }

    // Caricamento e controllo API Token
    len = sizeof(api_token);
    if (nvs_get_str(handle, NVS_API_TOKEN, api_token, &len) != ESP_OK || strlen(api_token) == 0) {
        strcpy(api_token, DEFAULT_API_TOKEN);
    }

    // Caricamento e controllo AskMeSign User
    len = sizeof(askmesign_user);
    if (nvs_get_str(handle, NVS_ASKMESIGN_USER, askmesign_user, &len) != ESP_OK || strlen(askmesign_user) == 0) {
        strcpy(askmesign_user, DEFAULT_ASKMESIGN_USER);
    }

    nvs_close(handle);

    // Log dei parametri caricati
    ESP_LOGI(TAG, "📶 SSID: %s", wifi_ssid);
    ESP_LOGI(TAG, "🔑 Password: %s", (strlen(wifi_password) > 0) ? "******" : "⚠️ Vuota!");
    ESP_LOGI(TAG, "🌍 Web Server: %s", web_server);
    ESP_LOGI(TAG, "🔌 Web Port: %s", web_port);
    ESP_LOGI(TAG, "🌐 Web URL: %s", web_url);
    ESP_LOGI(TAG, "🔑 API Token: %s", (strlen(api_token) > 0) ? "******" : "⚠️ Vuoto!");
    ESP_LOGI(TAG, "👤 AskMeSign User: %s", askmesign_user);
}


/**
 * Salva i parametri in NVS
 */
void save_config_to_nvs(const char *ssid, const char *password, const char *server, const char *port, const char *url, const char *token, const char *user) {
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Errore nell'apertura di NVS per la scrittura: %s", esp_err_to_name(err));
        return;
    }

    if (ssid && strlen(ssid) > 0) nvs_set_str(handle, NVS_WIFI_SSID, ssid);
    if (password && strlen(password) > 0) nvs_set_str(handle, NVS_WIFI_PASSWORD, password);
    if (server && strlen(server) > 0) nvs_set_str(handle, NVS_WEB_SERVER, server);
    if (port && strlen(port) > 0) nvs_set_str(handle, NVS_WEB_PORT, port);
    if (url && strlen(url) > 0) nvs_set_str(handle, NVS_WEB_URL, url);
    if (token && strlen(token) > 0) nvs_set_str(handle, NVS_API_TOKEN, token);
    if (user && strlen(user) > 0) nvs_set_str(handle, NVS_ASKMESIGN_USER, user);

    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "✅ Configurazioni salvate con successo in NVS!");
}