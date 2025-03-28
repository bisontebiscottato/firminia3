#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "device_config.h"      // Global configuration and functions: load_config_from_nvs(), save_config_to_nvs(), global config variables
#include "ble_manager.h"
#include "wifi_manager.h"
#include "api_manager.h"
#include "display_manager.h"

static const char* TAG = "MainFlow";

typedef enum {
    STATE_WARMING_UP,
    STATE_BLE_ADVERTISING,
    STATE_CONFIG_UPDATED,
    STATE_WIFI_CONNECTING,
    STATE_CHECKING_API,
    STATE_SHOW_PRACTICES,
    STATE_NO_PRACTICES,
    STATE_NO_WIFI
} app_state_t;

static app_state_t s_current_state = STATE_WARMING_UP;

// Callback invoked when JSON config is received via BLE
static void on_ble_config_received(const char* json_str)
{
    ESP_LOGI(TAG, "BLE config received: %s", json_str);
    // Parse the JSON and update configuration here.
    // For demonstration, we simulate updating configuration fields:
    strcpy(wifi_ssid, "MyHomeWiFi");
    strcpy(wifi_password, "SuperSecret");
    strcpy(web_url, "https://askmesign.askmesuite.com/api/v2/files/pending?page=0&size=1");
    strcpy(api_token, "1pvXudF9s67qukAz8slLrnpDgxAnkBZb74g1");
    strcpy(askmesign_user, "user@example.com");

    save_config_to_nvs();

    ble_manager_stop_advertising();

    s_current_state = STATE_CONFIG_UPDATED;
    display_manager_update(DISPLAY_STATE_CONFIG_UPDATED, 0);
    ESP_LOGI(TAG, "Configuration updated via BLE.");
    vTaskDelay(pdMS_TO_TICKS(2000));
}

// Simple check: if wifi_ssid equals the default, configuration is considered invalid.
static bool is_config_valid(void)
{
    return (strcmp(wifi_ssid, DEFAULT_WIFI_SSID) != 0);
}

static void main_flow_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Starting main flow task...");

    // WARMING UP
    s_current_state = STATE_WARMING_UP;
    display_manager_update(DISPLAY_STATE_WARMING_UP, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Load configuration from NVS
    ESP_LOGI(TAG, "Loading configuration from NVS...");
    load_config_from_nvs();
    bool config_valid = is_config_valid();
    ESP_LOGI(TAG, "Configuration valid: %s", config_valid ? "YES" : "NO");

    if (!config_valid) {
        s_current_state = STATE_BLE_ADVERTISING;
        display_manager_update(DISPLAY_STATE_BLE_ADVERTISING, 0);
        ble_manager_start_advertising();
        ble_manager_set_config_callback(on_ble_config_received);
        ESP_LOGI(TAG, "Waiting up to 5 seconds for BLE configuration...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        config_valid = is_config_valid();
        if (!config_valid) {
            ESP_LOGW(TAG, "No valid config received. Proceeding with default values.");
        }
    }

    if (config_valid) {
        // Connect to Wi-Fi
        s_current_state = STATE_WIFI_CONNECTING;
        display_manager_update(DISPLAY_STATE_WIFI_CONNECTING, 0);
        bool wifi_ok = wifi_manager_connect(wifi_ssid, wifi_password);
        if (!wifi_ok) {
            s_current_state = STATE_NO_WIFI;
            display_manager_update(DISPLAY_STATE_NO_WIFI_SLEEPING, 0);
            ESP_LOGW(TAG, "Wi-Fi connection failed. Retrying in loop...");
        }
    }

// Main loop: poll API and monitor Wi-Fi connection
while (1) {
    // Check Wi-Fi connection status; if lost, try to reconnect
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "Wi-Fi connection lost. Attempting reconnection...");
        display_manager_update(DISPLAY_STATE_WIFI_CONNECTING, 0);
        if (!wifi_manager_connect(wifi_ssid, wifi_password)) {
            ESP_LOGW(TAG, "Reconnection attempt failed.");
            display_manager_update(DISPLAY_STATE_NO_WIFI_SLEEPING, 0);
            vTaskDelay(pdMS_TO_TICKS(5000));
            continue;
        } else {
            ESP_LOGI(TAG, "Wi-Fi reconnected successfully.");
        }
    }

    // Double-check that Wi-Fi is connected before API call
    if (!wifi_manager_is_connected()) {
        ESP_LOGW(TAG, "API call skipped: no Wi-Fi connection.");
        display_manager_update(DISPLAY_STATE_NO_WIFI_SLEEPING, 0);
        vTaskDelay(pdMS_TO_TICKS(5000));
        continue;
    }

    s_current_state = STATE_CHECKING_API;
    display_manager_update(DISPLAY_STATE_CHECKING_API, 0);
    int practices = api_manager_check_practices(web_url, api_token);
    if (practices < 0) {
        ESP_LOGE(TAG, "API call failed (network issue, server error, or certificate issue).");
        display_manager_update(DISPLAY_STATE_API_ERROR, 0); // Mostra il nuovo stato per errori API
    } else if (practices > 0) {
        s_current_state = STATE_SHOW_PRACTICES;
        display_manager_update(DISPLAY_STATE_SHOW_PRACTICES, practices);
    } else {
        s_current_state = STATE_NO_PRACTICES;
        display_manager_update(DISPLAY_STATE_NO_PRACTICES, 0);
    }
    ESP_LOGI(TAG, "Waiting 10 seconds before next API check...");
    vTaskDelay(pdMS_TO_TICKS(100000));
}

}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ble_manager_init();
    wifi_manager_init();
    display_manager_init();

    xTaskCreate(main_flow_task, "main_flow_task", 8192, NULL, 5, NULL);
}
