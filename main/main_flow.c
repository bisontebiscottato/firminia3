/*************************************************************
 *                     FIRMINIA 3.5.3                        *
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
#include "ota_manager.h"
#include "translations.h"
 
 static const char* TAG = "MainFlow";
 
#define BUTTON_GPIO                    5
#define WARMUP_DURATION_MS             5000    // Duration of the warmup phase
#define POLL_INTERVAL_MS               200     // Polling interval of the button during warmup
#define BLE_WAIT_DURATION_MS           120000   // Maximum waiting time for BLE configuration
#define DEFAULT_API_CHECK_INTERVAL_MS  60000UL // Waiting time between one API check and the next
#define BUTTON_POLL_INTERVAL_MS        200     // Polling interval of the button in the waiting loop
#define OTA_BUTTON_HOLD_TIME_MS        5000    // Time to hold button for OTA update
#define RESET_BUTTON_HOLD_TIME_MS      10000   // Time to hold button for configuration reset
#define OTA_CHECK_INTERVAL_MS          3600000UL // OTA check every hour (for testing, normally 6 hours)

 bool force_immediate_check = false;   // se true, salta il periodo di attesa

// OTA variables
static uint32_t last_ota_check = 0;
static bool ota_in_progress = false;
static bool force_display_refresh = false;
#define CURRENT_FIRMWARE_VERSION "3.5.3"

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
 
// Global variable to track long press in progress
static bool g_long_press_in_progress = false;

// Function removed - logic integrated directly in the main loop


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

// OTA progress callback
static void ota_progress_callback(int percentage, ota_status_t status, ota_error_t error)
{
    const char* status_text = "";
    
    // Set display to OTA mode on first call
    static bool ota_display_set = false;
    if (!ota_display_set) {
        display_manager_update(DISPLAY_STATE_OTA_UPDATE, 0);
        ota_display_set = true;
    }
    
    // Get current language for translations
    language_t current_lang = get_current_language();
    
    switch (status) {
        case OTA_STATUS_CHECKING:
            status_text = get_translated_string(STR_OTA_CHECKING, current_lang);
            break;
        case OTA_STATUS_DOWNLOADING:
            status_text = get_translated_string(STR_OTA_DOWNLOADING, current_lang);
            break;
        case OTA_STATUS_VERIFYING:
            status_text = get_translated_string(STR_OTA_VERIFYING, current_lang);
            break;
        case OTA_STATUS_INSTALLING:
            status_text = get_translated_string(STR_OTA_INSTALLING, current_lang);
            break;
        case OTA_STATUS_SUCCESS:
            status_text = get_translated_string(STR_OTA_COMPLETE, current_lang);
            ota_in_progress = false;
            ota_display_set = false; // Reset for next OTA
            break;
        case OTA_STATUS_ERROR:
            switch (error) {
                case OTA_ERROR_HTTP_FAILED:
                    status_text = get_translated_string(STR_OTA_NETWORK_ERROR, current_lang);
                    break;
                case OTA_ERROR_DOWNLOAD_FAILED:
                    status_text = get_translated_string(STR_OTA_DOWNLOAD_FAILED, current_lang);
                    break;
                case OTA_ERROR_SIGNATURE_INVALID:
                    status_text = get_translated_string(STR_OTA_INVALID_SIGNATURE, current_lang);
                    break;
                default:
                    status_text = get_translated_string(STR_OTA_UPDATE_ERROR, current_lang);
                    break;
            }
            ota_in_progress = false;
            ota_display_set = false; // Reset for next OTA
            break;
        default:
            status_text = get_translated_string(STR_OTA_UPDATING, current_lang);
            break;
    }
    
    ESP_LOGI(TAG, "🔄 OTA Progress: %d%% - %s", percentage, status_text);
    display_manager_show_ota_progress(percentage, status_text);
    
    // Handle completion or error
    if (status == OTA_STATUS_SUCCESS) {
        ESP_LOGI(TAG, "✅ OTA update completed successfully! Restarting in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else if (status == OTA_STATUS_ERROR) {
        ESP_LOGE(TAG, "❌ OTA update failed with error: %d", error);
        // Resume normal operation after 5 seconds
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

// Check for OTA updates
static void check_ota_updates(void)
{
    if (ota_in_progress) {
        ESP_LOGI(TAG, "⏳ OTA already in progress, skipping check");
        return;
    }
    
    ESP_LOGI(TAG, "🔍 Checking for firmware updates...");
    
    ota_version_info_t update_info;
    esp_err_t err = api_manager_check_firmware_updates(CURRENT_FIRMWARE_VERSION, &update_info);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "🚀 Update available: %s → %s", CURRENT_FIRMWARE_VERSION, update_info.version);
        
        // Set OTA in progress flag
        ota_in_progress = true;
        
        // Show OTA state on display
        display_manager_update(DISPLAY_STATE_OTA_UPDATE, 0);
        
        // Start OTA update
        err = ota_start_update(&update_info);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to start OTA update: %s", esp_err_to_name(err));
            ota_in_progress = false;
        }
    } else if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "ℹ️ No firmware updates available");
        
        // Show "No Updates" message for 3 seconds
        display_manager_update(DISPLAY_STATE_NO_OTA_UPDATE, 0);
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // Force display refresh to return to normal state
        force_display_refresh = true;
        
    } else {
        ESP_LOGE(TAG, "❌ Failed to check for updates: %s", esp_err_to_name(err));
        
        // Show error message for 3 seconds
        display_manager_update(DISPLAY_STATE_API_ERROR, 0);
        vTaskDelay(pdMS_TO_TICKS(3000));
        
        // Force display refresh to return to normal state
        force_display_refresh = true;
    }
    
    last_ota_check = xTaskGetTickCount() * portTICK_PERIOD_MS;
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
        
        /* -- 0. Check for button actions in ALL states (OTA: 5s, Reset: 10s) --- */
        if (s_current_state != STATE_WARMING_UP) {
            // Check for direct button press or flag from API wait loop
            int current_button = gpio_get_level(BUTTON_GPIO);
            
            if (g_long_press_in_progress) {
                ESP_LOGI(TAG, "🚨 Long press flag detected from API wait loop!");
            }
            
            if (current_button == 1 || g_long_press_in_progress) {
            uint32_t hold_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
            uint32_t hold_time = 0;
            bool ota_triggered = false;
            bool was_long_press_detected = g_long_press_in_progress; // Remember the state
            
            // If long press was already detected by immediate_check_triggered, skip the wait
            if (g_long_press_in_progress) {
                ESP_LOGI(TAG, "🔘 Long press already detected - continuing with hold monitoring...");
                g_long_press_in_progress = false; // Reset the flag
                // Adjust hold_start to account for the 1 second already elapsed
                hold_start -= 1000;
            } else {
                // Wait 1 second to see if this is a short press (handled elsewhere) or long press
                ESP_LOGI(TAG, "🔘 Button pressed - waiting to distinguish short/long press...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                
                // If button was released during the 1-second wait, ignore (short press)
                if (gpio_get_level(BUTTON_GPIO) == 0) {
                    ESP_LOGI(TAG, "🔘 Button released during initial wait - ignoring (short press)");
                    continue;
                }
                
                ESP_LOGI(TAG, "🔘 Long press confirmed - monitoring hold duration...");
            }
            
            // Monitor button hold duration (starting from 1 second)
            while (gpio_get_level(BUTTON_GPIO) == 1) {
                vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
                hold_time = (xTaskGetTickCount() * portTICK_PERIOD_MS) - hold_start;
                
                // Log progress every second
                if (hold_time % 1000 == 0 && hold_time > 0) {
                    ESP_LOGI(TAG, "🔘 Button held for %lu ms...", hold_time);
                }
                
                // Trigger OTA at 4 seconds additional (5 seconds total: 1s wait + 4s hold)
                if (!ota_triggered && hold_time >= (OTA_BUTTON_HOLD_TIME_MS - 1000) && 
                    !ota_in_progress && wifi_manager_is_connected()) {
                    ESP_LOGI(TAG, "🚀 OTA update triggered after %lu ms total!", hold_time + 1000);
                    ota_triggered = true;
                    check_ota_updates();
                }
                
                // Trigger reset at 9 seconds additional (10 seconds total: 1s wait + 9s hold)
                if (hold_time >= (RESET_BUTTON_HOLD_TIME_MS - 1000)) {
                    ESP_LOGW(TAG, "🔄 Configuration reset triggered after %lu ms total!", hold_time + 1000);
                    reset_config_to_default();
                    vTaskDelay(pdMS_TO_TICKS(1000));
                    esp_restart();
                }
            }
            
            // Calculate total time 
            // If was_long_press_detected: 1 second already counted in hold_start adjustment
            // If not: add 1 second for the initial wait
            uint32_t total_time = hold_time + (was_long_press_detected ? 0 : 1000);
            ESP_LOGI(TAG, "🔘 Button released after %lu ms total", total_time);
            }
        }
        
        /* -- 0.5. Check for OTA updates (periodically, when WiFi connected) --- */
        if (!ota_in_progress && wifi_manager_is_connected()) {
            uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
            if (current_time - last_ota_check > OTA_CHECK_INTERVAL_MS) {
                ESP_LOGI(TAG, "⏰ Periodic OTA check triggered");
                check_ota_updates();
            }
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
                // Check for short button press for immediate API check
                if ((s_current_state == STATE_SHOW_PRACTICES ||
                    s_current_state == STATE_NO_PRACTICES ||
                    s_current_state == STATE_API_ERROR)) {
                    
                    int current = gpio_get_level(BUTTON_GPIO);
                    if (last_button_state == 0 && current == 1) {
                        ESP_LOGI(TAG, "🔘 Button press detected during API wait - checking duration...");
                        
                        // Wait up to 1 second to see if it's a short or long press
                        uint32_t press_start = xTaskGetTickCount() * portTICK_PERIOD_MS;
                        uint32_t hold_time = 0;
                        
                        while (gpio_get_level(BUTTON_GPIO) == 1 && hold_time < 1000) {
                            vTaskDelay(pdMS_TO_TICKS(50));
                            hold_time = (xTaskGetTickCount() * portTICK_PERIOD_MS) - press_start;
                            elapsed += 50; // Account for the delay in the main timer
                        }
                        
                        if (hold_time < 1000 && gpio_get_level(BUTTON_GPIO) == 0) {
                            ESP_LOGI(TAG, "🔘 Short press detected (%lu ms) - triggering immediate API check", hold_time);
                            last_button_state = 0;
                            break; // Exit wait loop for immediate API check
                        } else {
                            ESP_LOGI(TAG, "🔘 Long press detected (%lu ms) - setting flag for main loop", hold_time);
                            ESP_LOGI(TAG, "🔘 Current state: %d", s_current_state);
                            g_long_press_in_progress = true;
                            last_button_state = current;
                            break; // Exit wait loop immediately to let main loop handle long press
                        }
                    } else {
                        last_button_state = current;
                    }
                }
                
                vTaskDelay(pdMS_TO_TICKS(BUTTON_POLL_INTERVAL_MS));
                elapsed += BUTTON_POLL_INTERVAL_MS;
            }
        }
        /* Se force_immediate_check era true, saltiamo completamente il loop */

        force_immediate_check = false;         // consumato il "bonus" immediato

        /* -- 3. Controllo delle pratiche ------------------------------------ */
        // Skip API check if we have a long press to handle
        if (g_long_press_in_progress) {
            ESP_LOGI(TAG, "🔘 Skipping API check due to long press in progress");
            continue; // Go back to main loop start to handle the long press
        }
        
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

          // Check if we need to force display refresh after OTA check feedback
          if (force_display_refresh) {
              force_display_refresh = false;
              // Re-display current state to override temporary OTA messages
              switch (s_current_state) {
                  case STATE_SHOW_PRACTICES:
                      display_manager_update(DISPLAY_STATE_SHOW_PRACTICES, practices);
                      break;
                  case STATE_NO_PRACTICES:
                      display_manager_update(DISPLAY_STATE_NO_PRACTICES, 0);
                      break;
                  case STATE_API_ERROR:
                      display_manager_update(DISPLAY_STATE_API_ERROR, 0);
                      break;
                  default:
                      // For other states, just continue normal flow
                      break;
              }
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
    
    // Initialize OTA manager
    esp_err_t ota_err = ota_manager_init(ota_progress_callback);
    if (ota_err == ESP_OK) {
        // Mark current firmware as valid (in case we just updated)
        ota_mark_firmware_valid();
        ESP_LOGI(TAG, "✅ OTA Manager initialized successfully");
    } else {
        ESP_LOGE(TAG, "❌ Failed to initialize OTA Manager: %s", esp_err_to_name(ota_err));
    }

    xTaskCreate(main_flow_task, "main_flow_task", 8192, NULL, 5, NULL);
 }
 