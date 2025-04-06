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
#define WARMUP_DURATION_MS    3000    // Durata della fase di warmup
#define POLL_INTERVAL_MS      200      // Intervallo di polling del tasto durante il warmup
#define BLE_WAIT_DURATION_MS  30000   // Tempo massimo di attesa per la configurazione BLE
#define API_CHECK_INTERVAL_MS 60000   // Tempo di attesa tra un controllo API e l'altro
#define BUTTON_POLL_INTERVAL_MS 200  // Intervallo per il polling del tasto nel ciclo di attesa

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

// Callback per BLE (il parsing è gestito in ble_process_received_data)
static void on_ble_config_received(const char* json_str)
{
    ESP_LOGI(TAG, "BLE config received (callback): %s", json_str);
    // Il parsing e l'aggiornamento avvengono in ble_process_received_data.
}

// La configurazione è valida se wifi_ssid non è uguale al default.
static bool is_config_valid(void)
{
    return (strcmp(wifi_ssid, DEFAULT_WIFI_SSID) != 0);
}

// Funzione che controlla il tasto durante il warmup
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

// Funzione per rilevare il rising edge del tasto.
// last_state è un puntatore allo stato del tasto dalla lettura precedente.
static bool immediate_check_triggered(int *last_state)
{
    int current = gpio_get_level(BUTTON_GPIO);
    bool triggered = false;
    if (*last_state == 0 && current == 1) {
         triggered = true;
    }
    *last_state = current;
    return triggered;
}

static void main_flow_task(void* pvParameters)
{

    
    ESP_LOGI(TAG, "Starting main flow task...");

    // Fase di WARMING UP
    s_current_state = STATE_WARMING_UP;
    display_manager_update(DISPLAY_STATE_WARMING_UP, 0);

    if (check_button_pressed_during_warmup()) {
         ESP_LOGI(TAG, "Button press detected during warmup. Entering BLE configuration mode.");
         s_current_state = STATE_BLE_ADVERTISING;
         display_manager_update(DISPLAY_STATE_BLE_ADVERTISING, 0);

         // Reset della configurazione attuale
         strcpy(wifi_ssid, DEFAULT_WIFI_SSID);
         save_config_to_nvs();

         ble_manager_start_advertising();
         ble_manager_set_config_callback(on_ble_config_received);

         uint32_t waited = 0;
         while (waited < BLE_WAIT_DURATION_MS && !is_config_valid()) {
             vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
             waited += POLL_INTERVAL_MS;
         }
         if (is_config_valid()) {
             ESP_LOGI(TAG, "Valid configuration received via BLE, stopping advertising and restarting system.");
             ble_manager_stop_advertising();
             ble_manager_disconnect();
             esp_restart();
         } else {
             ESP_LOGW(TAG, "No valid BLE configuration received, proceeding with normal operation.");
             ble_manager_stop_advertising();
         }
    } else {
         vTaskDelay(pdMS_TO_TICKS(WARMUP_DURATION_MS));
    }

    // Carica la configurazione da NVS
    ESP_LOGI(TAG, "Loading configuration from NVS...");
    load_config_from_nvs();
    bool config_valid = is_config_valid();
    ESP_LOGI(TAG, "Configuration valid: %s", config_valid ? "YES" : "NO");

    if (config_valid) {
         s_current_state = STATE_WIFI_CONNECTING;
         display_manager_update(DISPLAY_STATE_WIFI_CONNECTING, 0);
         bool wifi_ok = wifi_manager_connect(wifi_ssid, wifi_password);
         if (!wifi_ok) {
              s_current_state = STATE_NO_WIFI;
              display_manager_update(DISPLAY_STATE_NO_WIFI_SLEEPING, 0);
              ESP_LOGW(TAG, "Wi-Fi connection failed. Retrying in loop...");
         }
    }

    // Inizializza la variabile per il rilevamento del rising edge
    int last_button_state = gpio_get_level(BUTTON_GPIO);

    while (1) {
         // Verifica la connessione Wi-Fi
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

         // Attendi API_CHECK_INTERVAL_MS, ma controlla periodicamente il tasto
         uint32_t elapsed = 0;
         while (elapsed < API_CHECK_INTERVAL_MS) {
             // Se siamo in STATE_SHOW_PRACTICES o STATE_NO_PRACTICES e viene rilevato il rising edge...
             if ((s_current_state == STATE_SHOW_PRACTICES || s_current_state == STATE_NO_PRACTICES) &&
                 immediate_check_triggered(&last_button_state))
             {
                 ESP_LOGI(TAG, "Rising edge detected: triggering immediate API check.");
                 break;  // Esce dal ciclo per eseguire subito il controllo API
             }
             vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
             elapsed += BUTTON_POLL_INTERVAL_MS;
         }

         // Esegui un solo controllo API per ciclo
         s_current_state = STATE_CHECKING_API;
         display_manager_update(DISPLAY_STATE_CHECKING_API, 0);
         int practices = api_manager_check_practices();
         ESP_LOGI(TAG, "practices = %d", practices);

         if (practices < 0) {
             ESP_LOGE(TAG, "API call failed (network issue, server error, or certificate issue).");
             display_manager_update(DISPLAY_STATE_API_ERROR, 0);
         }
         else if (practices > 0) {
             s_current_state = STATE_SHOW_PRACTICES;
             ESP_LOGI(TAG, "Switching state to SHOW_PRACTICES");
             display_manager_update(DISPLAY_STATE_SHOW_PRACTICES, practices);
         }
         else {
             s_current_state = STATE_NO_PRACTICES;
             display_manager_update(DISPLAY_STATE_NO_PRACTICES, 0);
         }

         // Breve delay per dare tempo agli altri task (es. LVGL) e alimentare il watchdog
         vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_LOGE(TAG, "Main flow task exiting unexpectedly.");
    vTaskDelete(NULL);
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
         ESP_ERROR_CHECK(nvs_flash_erase());
         ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    gpio_config_t btn_config = {
         .pin_bit_mask = (1ULL << BUTTON_GPIO),
         .mode = GPIO_MODE_INPUT,
         .pull_up_en = GPIO_PULLUP_DISABLE,
         .pull_down_en = GPIO_PULLDOWN_ENABLE,
         .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_config);

    ble_manager_init();
    wifi_manager_init();
    display_manager_init();

    xTaskCreate(main_flow_task, "main_flow_task", 8192, NULL, 5, NULL);
}
