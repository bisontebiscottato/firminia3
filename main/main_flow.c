/*************************************************************
 *                     FIRMINIA 3.5.1                        *
 *  File: main_flow.c                                        *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 *                                                            *
 *  Configuration Reset Feature:                             *
 *  - Hold button for 5 seconds during any state except      *
 *    WARMING_UP to reset configuration to defaults          *
 *  - System automatically enters BLE mode when default      *
 *    configuration is detected                               *
 ************************************************************/

 #include <stdio.h>
 #include <string.h>
 #include "esp_log.h"
 #include "nvs_flash.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "driver/gpio.h"
 
 #include "device_config.h"   // For loading and saving NVS configuration
 #include "ble_manager.h"
 #include "wifi_manager.h"
 #include "api_manager.h"
 #include "display_manager.h"
 
 static const char* TAG = "MainFlow";
 
#define BUTTON_GPIO                    5
#define WARMUP_DURATION_MS             5000    // Duration of the warmup phase
#define POLL_INTERVAL_MS               200     // Polling interval of the button during warmup
#define BLE_WAIT_DURATION_MS           120000   // Maximum waiting time for BLE configuration
#define DEFAULT_API_CHECK_INTERVAL_MS  60000UL // Waiting time between one API check and the next
#define BUTTON_POLL_INTERVAL_MS        200     // Polling interval of the button in the waiting loop
#define RESET_BUTTON_HOLD_TIME_MS      5000    // Time to hold button for configuration reset

 bool force_immediate_check = false;   // se true, salta il periodo di attesa

 typedef enum {
    STATE_WARMING_UP,
    STATE_BLE_ADVERTISING,
    STATE_CONFIG_UPDATED,
    STATE_WIFI_CONNECTING,
    STATE_CHECKING_API,
    STATE_SHOW_PRACTICES,
    STATE_NO_PRACTICES,
    STATE_NO_WIFI,
    STATE_API_ERROR
} app_state_t;
 
 static app_state_t s_current_state = STATE_WARMING_UP;
 static bool s_config_received_flag = false;
 
 // Callback for BLE (parsing is handled in ble_process_received_data)
 static void on_ble_config_received(const char* json_str)
 {
     ESP_LOGI(TAG, "BLE config received (callback): %s", json_str);
     // Set flag to indicate new configuration was received
     s_config_received_flag = true;
     // Parsing and updating take place in ble_process_received_data.
 }
 
// The configuration is valid if it's not using default values
static bool is_config_valid(void)
{
    return !is_config_default();
}
 
 // Function that checks the button during warmup
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
 
// Function to detect the rising edge of the button.
// last_state is a pointer to the button state from the previous reading.
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

// Function to check if button is held for reset duration
// Returns true if button was held for RESET_BUTTON_HOLD_TIME_MS
static bool check_button_held_for_reset(void)
{
    if (gpio_get_level(BUTTON_GPIO) == 0) {
        return false; // Button not pressed
    }
    
    ESP_LOGI(TAG, "🔘 Button press detected, checking for reset hold...");
    uint32_t hold_time = 0;
    
    // Check if button remains pressed for the required duration
    while (gpio_get_level(BUTTON_GPIO) == 1 && hold_time < RESET_BUTTON_HOLD_TIME_MS) {
        vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
        hold_time += BUTTON_POLL_INTERVAL_MS;
        
        // Log progress every second
        if (hold_time % 1000 == 0) {
            ESP_LOGI(TAG, "🔘 Button held for %lu/%d ms...", hold_time, RESET_BUTTON_HOLD_TIME_MS);
        }
    }
    
    if (hold_time >= RESET_BUTTON_HOLD_TIME_MS) {
        ESP_LOGW(TAG, "🔄 Button held for %lu ms - Configuration reset triggered!", hold_time);
        return true;
    } else {
        ESP_LOGI(TAG, "🔘 Button released after %lu ms - Reset cancelled", hold_time);
        return false;
    }
}
 
 static void main_flow_task(void* pvParameters)
 {
 
     
     ESP_LOGI(TAG, "Starting main flow task...");
 
     // WARMING UP phase
     s_current_state = STATE_WARMING_UP;
     display_manager_update(DISPLAY_STATE_WARMING_UP, 0);
 
          if (check_button_pressed_during_warmup()) {
          ESP_LOGI(TAG, "Button press detected during warmup. Entering BLE configuration mode.");
          s_current_state = STATE_BLE_ADVERTISING;
          display_manager_update(DISPLAY_STATE_BLE_ADVERTISING, 0);

         // Never modify saved configuration until valid BLE config is received
         ESP_LOGI(TAG, "Entering BLE mode - existing configuration preserved until valid config received");
 
          ble_manager_start_advertising();
          ble_manager_set_config_callback(on_ble_config_received);
 
          uint32_t waited = 0;
          bool new_config_received = false;
          
          // Always wait for BLE configuration, regardless of current config validity
          while (waited < BLE_WAIT_DURATION_MS && !new_config_received) {
              vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
              waited += POLL_INTERVAL_MS;
              
              // Check for configuration reset during BLE mode
              if (check_button_held_for_reset()) {
                  ESP_LOGW(TAG, "🔄 Configuration reset requested during BLE mode - resetting and restarting...");
                  ble_manager_stop_advertising();
                  ble_manager_disconnect();
                  reset_config_to_default();
                  vTaskDelay(pdMS_TO_TICKS(1000));
                  esp_restart();
              }
              
              // Check if a new configuration was received via BLE callback
              // This will be set to true in the BLE callback when valid config is received
              if (s_config_received_flag) {
                  new_config_received = true;
                  s_config_received_flag = false; // Reset flag
              }
          }
          
          if (new_config_received) {
              ESP_LOGI(TAG, "New configuration received via BLE, stopping advertising and restarting system.");
              ble_manager_stop_advertising();
              ble_manager_disconnect();
              
              // Add a small delay to ensure configuration is properly saved
              vTaskDelay(pdMS_TO_TICKS(100));
              
              esp_restart();
          } else {
              ESP_LOGW(TAG, "No new BLE configuration received, proceeding with existing configuration.");
              ble_manager_stop_advertising();
          }
     } else {
          vTaskDelay(pdMS_TO_TICKS(WARMUP_DURATION_MS));
     }

     // Load configuration from NVS
     ESP_LOGI(TAG, "Loading configuration from NVS...");
     load_config_from_nvs();
     bool config_valid = is_config_valid();
     ESP_LOGI(TAG, "Configuration valid: %s", config_valid ? "YES" : "NO");

     // If configuration is default/invalid, automatically enter BLE mode
     if (!config_valid) {
         ESP_LOGW(TAG, "Default configuration detected. Automatically entering BLE configuration mode.");
         s_current_state = STATE_BLE_ADVERTISING;
         display_manager_update(DISPLAY_STATE_BLE_ADVERTISING, 0);

         ble_manager_start_advertising();
         ble_manager_set_config_callback(on_ble_config_received);

         uint32_t waited = 0;
         bool new_config_received = false;
         
         // Wait indefinitely for BLE configuration when using default config
         while (!new_config_received) {
             vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
             waited += POLL_INTERVAL_MS;
             
             // Check for configuration reset during automatic BLE mode
             if (check_button_held_for_reset()) {
                 ESP_LOGW(TAG, "🔄 Configuration reset requested during automatic BLE mode - resetting and restarting...");
                 ble_manager_stop_advertising();
                 ble_manager_disconnect();
                 reset_config_to_default();
                 vTaskDelay(pdMS_TO_TICKS(1000));
                 esp_restart();
             }
             
             // Check if a new configuration was received via BLE callback
             if (s_config_received_flag) {
                 new_config_received = true;
                 s_config_received_flag = false; // Reset flag
             }

             // Log status every 30 seconds
             if (waited % 30000 == 0) {
                 ESP_LOGI(TAG, "Waiting for BLE configuration... (%lu seconds elapsed)", waited / 1000);
             }
         }
         
         if (new_config_received) {
             ESP_LOGI(TAG, "New configuration received via BLE, stopping advertising and restarting system.");
             ble_manager_stop_advertising();
             ble_manager_disconnect();
             
             // Add a small delay to ensure configuration is properly saved
             vTaskDelay(pdMS_TO_TICKS(100));
             
             esp_restart();
         }
     }
 
     if (config_valid) {
          s_current_state = STATE_WIFI_CONNECTING;
          display_manager_update(DISPLAY_STATE_WIFI_CONNECTING, 0);
          bool wifi_ok = wifi_manager_connect(wifi_ssid, wifi_password);
            if (wifi_ok) {
                force_immediate_check = true;   // primo check immediato
            } else {
                s_current_state = STATE_NO_WIFI;
                display_manager_update(DISPLAY_STATE_NO_WIFI_SLEEPING, 0);
                ESP_LOGW(TAG, "Wi-Fi connection failed. Retrying in loop...");
          }
     }
 
     // Initialize the variable for rising edge detection
     int last_button_state = gpio_get_level(BUTTON_GPIO);
 
    while (1) {

        /* -- 0. Check for configuration reset (except during WARMING_UP) --- */
        if (s_current_state != STATE_WARMING_UP && check_button_held_for_reset()) {
            ESP_LOGW(TAG, "🔄 Configuration reset requested - resetting to defaults and restarting...");
            reset_config_to_default();
            vTaskDelay(pdMS_TO_TICKS(1000)); // Give time for user to see the message
            esp_restart();
        }

        /* -- 1. Gestione Wi-Fi ---------------------------------------------- */
        if (!wifi_manager_is_connected()) {
            ESP_LOGW(TAG, "Wi-Fi connection lost. Attempting reconnection…");
            display_manager_update(DISPLAY_STATE_WIFI_CONNECTING, 0);

            if (!wifi_manager_connect(wifi_ssid, wifi_password)) {
                ESP_LOGW(TAG, "Reconnection attempt failed.");
                display_manager_update(DISPLAY_STATE_NO_WIFI_SLEEPING, 0);
                vTaskDelay(pdMS_TO_TICKS(5000));
                continue;                      // riprova dall’inizio del while
            } else {
                ESP_LOGI(TAG, "Wi-Fi reconnected successfully.");
                force_immediate_check = true;  // ← qui!
            }
        }

        /* -- 2. Attesa o check immediato ------------------------------------ */
        if (!force_immediate_check) {          // attesa “tradizionale”
            uint32_t elapsed = 0;
            char *endptr;
            uint32_t interval = strtoul(api_interval_ms, &endptr, 10);

            if (endptr == api_interval_ms || *endptr != '\0') {
                interval = DEFAULT_API_CHECK_INTERVAL_MS; // se conversione fallita, uso il default
            }
            while (elapsed < interval) {
                if ((s_current_state == STATE_SHOW_PRACTICES ||
                    s_current_state == STATE_NO_PRACTICES ||
                    s_current_state == STATE_API_ERROR) &&
                    immediate_check_triggered(&last_button_state)) {
                    ESP_LOGI(TAG, "Button rising edge: immediate API check.");
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
                elapsed += BUTTON_POLL_INTERVAL_MS;
            }
        }
        /* Se force_immediate_check era true, saltiamo completamente il loop */

        force_immediate_check = false;         // consumato il “bonus” immediato

        /* -- 3. Controllo delle pratiche ------------------------------------ */
        s_current_state = STATE_CHECKING_API;
        display_manager_update(DISPLAY_STATE_CHECKING_API, 0);
        int practices = api_manager_check_practices();
          ESP_LOGI(TAG, "practices = %d", practices);
 
          if (practices < 0) {
              ESP_LOGE(TAG, "API call failed (network issue, server error, or certificate issue).");
              s_current_state = STATE_API_ERROR;
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
 
          // Brief delay to allow time for other tasks (e.g. LVGL) and feed the watchdog
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
 