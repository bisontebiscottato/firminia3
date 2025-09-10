/*************************************************************
 *                     FIRMINIA 3.5.2                          *
 *  File: ota_integration_example.c                         *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 *  Description: Example OTA integration for main_flow      *
 ************************************************************/

#include "ota_manager.h"
#include "api_manager.h"
#include "display_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "OTA_INTEGRATION";

// OTA check timer (check every 6 hours)
#define OTA_CHECK_INTERVAL_MS (6 * 60 * 60 * 1000)
static TimerHandle_t ota_check_timer = NULL;

// Current firmware version
#define CURRENT_FIRMWARE_VERSION "3.5.2"

// OTA progress callback
static void ota_progress_callback(int percentage, ota_status_t status, ota_error_t error)
{
    const char* status_text = "";
    
    switch (status) {
        case OTA_STATUS_CHECKING:
            status_text = "Checking...";
            break;
        case OTA_STATUS_DOWNLOADING:
            status_text = "Downloading...";
            break;
        case OTA_STATUS_VERIFYING:
            status_text = "Verifying...";
            break;
        case OTA_STATUS_INSTALLING:
            status_text = "Installing...";
            break;
        case OTA_STATUS_SUCCESS:
            status_text = "Complete!";
            break;
        case OTA_STATUS_ERROR:
            switch (error) {
                case OTA_ERROR_HTTP_FAILED:
                    status_text = "Network Error";
                    break;
                case OTA_ERROR_DOWNLOAD_FAILED:
                    status_text = "Download Failed";
                    break;
                case OTA_ERROR_SIGNATURE_INVALID:
                    status_text = "Invalid Signature";
                    break;
                default:
                    status_text = "Update Error";
                    break;
            }
            break;
        default:
            status_text = "Updating...";
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
        // You might want to notify the main flow to resume normal display
    }
}

// OTA check timer callback
static void ota_check_timer_callback(TimerHandle_t xTimer)
{
    ESP_LOGI(TAG, "🕐 Periodic OTA check triggered");
    
    // Check if we're in a suitable state for OTA (not in BLE mode, etc.)
    ota_status_t current_status = ota_get_status();
    if (current_status != OTA_STATUS_IDLE) {
        ESP_LOGI(TAG, "⏳ OTA already in progress, skipping check");
        return;
    }
    
    // Check for updates
    ota_version_info_t update_info;
    esp_err_t err = api_manager_check_firmware_updates(CURRENT_FIRMWARE_VERSION, &update_info);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "🚀 Starting automatic OTA update to version %s", update_info.version);
        
        // Show OTA state on display
        display_manager_update(DISPLAY_STATE_OTA_UPDATE, 0);
        
        // Start OTA update
        err = ota_start_update(&update_info);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to start OTA update: %s", esp_err_to_name(err));
        }
    } else if (err == ESP_ERR_NOT_FOUND) {
        ESP_LOGI(TAG, "ℹ️ No firmware updates available");
    } else {
        ESP_LOGE(TAG, "❌ Failed to check for updates: %s", esp_err_to_name(err));
    }
}

// Initialize OTA integration
esp_err_t ota_integration_init(void)
{
    ESP_LOGI(TAG, "🔧 Initializing OTA integration...");
    
    // Initialize OTA manager
    esp_err_t err = ota_manager_init(ota_progress_callback);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to initialize OTA manager: %s", esp_err_to_name(err));
        return err;
    }
    
    // Mark current firmware as valid (in case we just updated)
    ota_mark_firmware_valid();
    
    // Create periodic OTA check timer
    ota_check_timer = xTimerCreate(
        "ota_check_timer",
        pdMS_TO_TICKS(OTA_CHECK_INTERVAL_MS),
        pdTRUE,  // Auto-reload
        NULL,    // Timer ID
        ota_check_timer_callback
    );
    
    if (ota_check_timer == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create OTA check timer");
        return ESP_ERR_NO_MEM;
    }
    
    // Start the timer
    if (xTimerStart(ota_check_timer, 0) != pdPASS) {
        ESP_LOGE(TAG, "❌ Failed to start OTA check timer");
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "✅ OTA integration initialized successfully");
    ESP_LOGI(TAG, "📅 Next OTA check in %d hours", OTA_CHECK_INTERVAL_MS / (60 * 60 * 1000));
    
    return ESP_OK;
}

// Manual OTA check (can be called from button press or other trigger)
esp_err_t ota_integration_manual_check(void)
{
    ESP_LOGI(TAG, "🔍 Manual OTA check requested");
    
    ota_status_t current_status = ota_get_status();
    if (current_status != OTA_STATUS_IDLE) {
        ESP_LOGI(TAG, "⏳ OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Trigger immediate check
    ota_check_timer_callback(ota_check_timer);
    
    return ESP_OK;
}

// Get current OTA status for display
const char* ota_integration_get_status_string(void)
{
    ota_status_t status = ota_get_status();
    
    switch (status) {
        case OTA_STATUS_IDLE: return "Ready";
        case OTA_STATUS_CHECKING: return "Checking";
        case OTA_STATUS_DOWNLOADING: return "Downloading";
        case OTA_STATUS_VERIFYING: return "Verifying";
        case OTA_STATUS_INSTALLING: return "Installing";
        case OTA_STATUS_SUCCESS: return "Success";
        case OTA_STATUS_ERROR: return "Error";
        default: return "Unknown";
    }
}
