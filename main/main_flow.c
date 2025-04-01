#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#include "device_config.h"    // Funzioni load_config_from_nvs(), save_config_to_nvs() e variabili globali (wifi_ssid, ecc.)
#include "ble_manager.h"
#include "wifi_manager.h"
#include "api_manager.h"
#include "display_manager.h"

static const char* TAG = "MainFlow";

#define BUTTON_GPIO           5
#define WARMUP_DURATION_MS    2000   // Durata della fase di warmup
#define POLL_INTERVAL_MS      50     // Intervallo di polling del pulsante durante il warmup
#define BLE_WAIT_DURATION_MS  30000  // Tempo massimo di attesa per la configurazione BLE

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

// Callback chiamata quando arriva una configurazione via BLE.
// In questo esempio la funzione si limita a loggare il ricevimento; il parsing e l'aggiornamento avvengono in ble_process_received_data.
static void on_ble_config_received(const char* json_str)
{
    ESP_LOGI(TAG, "BLE config received (callback): %s", json_str);
    // Non facciamo riavvio qui: la funzione ble_process_received_data gestisce l'aggiornamento.
}

// La configurazione è considerata valida se il wifi_ssid non corrisponde al valore di default.
static bool is_config_valid(void)
{
    return (strcmp(wifi_ssid, DEFAULT_WIFI_SSID) != 0);
}

// Durante il warmup, controlla periodicamente se il pulsante viene premuto.
// Per la logica invertita (pull-down), il pulsante è premuto se gpio_get_level restituisce 1.
static bool check_button_pressed_during_warmup(void)
{
    int iterations = WARMUP_DURATION_MS / POLL_INTERVAL_MS;
    for (int i = 0; i < iterations; i++) {
         if (gpio_get_level(BUTTON_GPIO) == 1) {
             return true;
         }
         vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
    return false;
}

static void main_flow_task(void* pvParameters)
{
    ESP_LOGI(TAG, "Starting main flow task...");

    // Fase di WARMING UP
    s_current_state = STATE_WARMING_UP;
    display_manager_update(DISPLAY_STATE_WARMING_UP, 0);

    // Durante il warmup, esegue il polling per verificare se il pulsante viene premuto (anche brevemente)
    if (check_button_pressed_during_warmup()) {
         ESP_LOGI(TAG, "Button press detected during warmup. Entering BLE configuration mode.");
         s_current_state = STATE_BLE_ADVERTISING;
         display_manager_update(DISPLAY_STATE_BLE_ADVERTISING, 0);

         // Forza il reset della configurazione esistente
         strcpy(wifi_ssid, DEFAULT_WIFI_SSID);
         save_config_to_nvs();

         ble_manager_start_advertising();
         ble_manager_set_config_callback(on_ble_config_received);

         // Attende fino a BLE_WAIT_DURATION_MS oppure finché non viene ricevuta una configurazione valida
         uint32_t waited = 0;
         while (waited < BLE_WAIT_DURATION_MS && !is_config_valid()) {
             vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
             waited += POLL_INTERVAL_MS;
         }
         // Se la configurazione è diventata valida prima del timeout, interrompe l'advertising e riavvia il sistema.
         if (is_config_valid()) {
             ESP_LOGI(TAG, "Valid configuration received via BLE, stopping advertising and restarting system.");
             ble_manager_stop_advertising();
             esp_restart();
         } else {
             ESP_LOGW(TAG, "No valid BLE configuration received, proceeding with normal operation.");
             ble_manager_stop_advertising();
         }
    } else {
         // Se il pulsante non viene premuto, attende la durata completa della fase di warmup
         vTaskDelay(pdMS_TO_TICKS(WARMUP_DURATION_MS));
    }

    // Carica la configurazione da NVS
    ESP_LOGI(TAG, "Loading configuration from NVS...");
    load_config_from_nvs();
    bool config_valid = is_config_valid();
    ESP_LOGI(TAG, "Configuration valid: %s", config_valid ? "YES" : "NO");

    if (config_valid) {
         // Connessione Wi-Fi
         s_current_state = STATE_WIFI_CONNECTING;
         display_manager_update(DISPLAY_STATE_WIFI_CONNECTING, 0);
         bool wifi_ok = wifi_manager_connect(wifi_ssid, wifi_password);
         if (!wifi_ok) {
              s_current_state = STATE_NO_WIFI;
              display_manager_update(DISPLAY_STATE_NO_WIFI_SLEEPING, 0);
              ESP_LOGW(TAG, "Wi-Fi connection failed. Retrying in loop...");
         }
    }

    // Ciclo principale: controlla la connessione Wi-Fi e interroga l'API
    while (1) {
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

         s_current_state = STATE_CHECKING_API;
         display_manager_update(DISPLAY_STATE_CHECKING_API, 0);
         int practices = api_manager_check_practices();
         if (practices < 0) {
             ESP_LOGE(TAG, "API call failed (network issue, server error, or certificate issue).");
             display_manager_update(DISPLAY_STATE_API_ERROR, 0);
         }
         else if (practices > 0) {
             s_current_state = STATE_SHOW_PRACTICES;
             display_manager_update(DISPLAY_STATE_SHOW_PRACTICES, practices);
         }
         else {
             s_current_state = STATE_NO_PRACTICES;
             display_manager_update(DISPLAY_STATE_NO_PRACTICES, 0);
         }
         ESP_LOGI(TAG, "Waiting 10 seconds before next API check...");
         vTaskDelay(pdMS_TO_TICKS(10000));
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

    // Configura il GPIO del pulsante: logica invertita, il livello è normalmente basso (grazie al pull-down);
    // quando il pulsante viene premuto, il livello sale a 1.
    gpio_config_t btn_config = {
         .pin_bit_mask = (1ULL << BUTTON_GPIO),
         .mode = GPIO_MODE_INPUT,
         .pull_up_en = GPIO_PULLUP_DISABLE,
         .pull_down_en = GPIO_PULLDOWN_ENABLE,
         .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_config);

    // Inizializza i moduli BLE, Wi-Fi e display
    ble_manager_init();
    wifi_manager_init();
    display_manager_init();

    xTaskCreate(main_flow_task, "main_flow_task", 8192, NULL, 5, NULL);
}
