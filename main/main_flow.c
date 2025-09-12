/*************************************************************
 *                     FIRMINIA 3.5.4                        *
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
#include "esp_ota_ops.h"
#include "esp_partition.h"
 
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
#define BOOT_WATCHDOG_TIMEOUT_MS       30000   // 30 seconds timeout for boot completion
#define BOOT_HEALTH_CHECK_INTERVAL_MS  5000    // Check every 5 seconds during boot

// Test mode configuration
#define ENABLE_ROLLBACK_TESTS          1       // Set to 1 to enable test functions
#define TEST_FIRMWARE_CORRUPTION       0       // Test 1: Corrupted firmware (CRASH)
#define TEST_PROBLEMATIC_FIRMWARE      0       // Test 1b: Problematic firmware (SAFER)
#define TEST_FIRMWARE_VALIDATION       1       // Test 2: Firmware validation

 bool force_immediate_check = false;   // se true, salta il periodo di attesa

// OTA variables
static uint32_t last_ota_check = 0;
static bool ota_in_progress = false;
static bool force_display_refresh = false;
#define CURRENT_FIRMWARE_VERSION "3.5.4"

// Boot watchdog variables
static uint32_t boot_start_time = 0;
static bool boot_watchdog_active = false;
static uint32_t last_boot_health_check = 0;

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

// Forward declarations

// Enhanced logging for rollback operations
static void log_rollback_info(const char* operation, esp_err_t result)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_t* update = esp_ota_get_next_update_partition(NULL);
    
    ESP_LOGI(TAG, "📊 Rollback Debug Info - %s:", operation);
    ESP_LOGI(TAG, "  - Result: %s", esp_err_to_name(result));
    
    if (running) {
        ESP_LOGI(TAG, "  - Running partition: %s (0x%08lx, %ld bytes)", 
                 running->label, running->address, running->size);
    } else {
        ESP_LOGE(TAG, "  - Running partition: NULL");
    }
    
    if (update) {
        ESP_LOGI(TAG, "  - Update partition: %s (0x%08lx, %ld bytes)", 
                 update->label, update->address, update->size);
    } else {
        ESP_LOGE(TAG, "  - Update partition: NULL");
    }
    
    // Log partition states
    if (running) {
        esp_ota_img_states_t ota_state;
        if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
            const char* state_str = "UNKNOWN";
            switch (ota_state) {
                case ESP_OTA_IMG_VALID: state_str = "VALID"; break;
                case ESP_OTA_IMG_INVALID: state_str = "INVALID"; break;
                case ESP_OTA_IMG_ABORTED: state_str = "ABORTED"; break;
                case ESP_OTA_IMG_NEW: state_str = "NEW"; break;
                case ESP_OTA_IMG_PENDING_VERIFY: state_str = "PENDING_VERIFY"; break;
                case ESP_OTA_IMG_UNDEFINED: state_str = "UNDEFINED"; break;
                default: state_str = "UNKNOWN"; break;
            }
            ESP_LOGI(TAG, "  - Running partition state: %s (%d)", state_str, ota_state);
        }
    }
    
    // Log boot count and reset reason
    esp_reset_reason_t reset_reason = esp_reset_reason();
    const char* reset_str = "UNKNOWN";
    switch (reset_reason) {
        case ESP_RST_UNKNOWN: reset_str = "UNKNOWN"; break;
        case ESP_RST_POWERON: reset_str = "POWERON"; break;
        case ESP_RST_EXT: reset_str = "EXT"; break;
        case ESP_RST_SW: reset_str = "SW"; break;
        case ESP_RST_PANIC: reset_str = "PANIC"; break;
        case ESP_RST_INT_WDT: reset_str = "INT_WDT"; break;
        case ESP_RST_TASK_WDT: reset_str = "TASK_WDT"; break;
        case ESP_RST_WDT: reset_str = "WDT"; break;
        case ESP_RST_DEEPSLEEP: reset_str = "DEEPSLEEP"; break;
        case ESP_RST_BROWNOUT: reset_str = "BROWNOUT"; break;
        case ESP_RST_SDIO: reset_str = "SDIO"; break;
        case ESP_RST_USB: reset_str = "USB"; break;
        case ESP_RST_JTAG: reset_str = "JTAG"; break;
        case ESP_RST_EFUSE: reset_str = "EFUSE"; break;
        case ESP_RST_PWR_GLITCH: reset_str = "PWR_GLITCH"; break;
        case ESP_RST_CPU_LOCKUP: reset_str = "CPU_LOCKUP"; break;
        default: reset_str = "UNKNOWN"; break;
    }
    ESP_LOGI(TAG, "  - Reset reason: %s (%d)", reset_str, reset_reason);
    
    // Log free heap
    ESP_LOGI(TAG, "  - Free heap: %ld bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "  - Min free heap: %ld bytes", esp_get_minimum_free_heap_size());
}
 
 // Check if configuration is using default values
 static bool is_config_default(void)
 {
     return (strcmp(wifi_ssid, DEFAULT_WIFI_SSID) == 0);
 }
 
 // The configuration is valid if it's not using default values
 static bool is_config_valid(void)
 {
     return !is_config_default();
 }
 
// Reset configuration to default values
static void reset_config_to_default(void)
{
    ESP_LOGW(TAG, "🔄 Resetting configuration to default values...");
    
    // Reset all configuration variables to defaults
    strcpy(wifi_ssid, DEFAULT_WIFI_SSID);
    strcpy(wifi_password, DEFAULT_WIFI_PASSWORD);
    strcpy(web_server, DEFAULT_WEB_SERVER);
    strcpy(web_port, DEFAULT_WEB_PORT);
    strcpy(web_url, DEFAULT_WEB_URL);
    strcpy(api_token, DEFAULT_API_TOKEN);
    strcpy(askmesign_user, DEFAULT_ASKMESIGN_USER);
    strcpy(api_interval_ms, DEFAULT_API_INTERVAL_MS);
    strcpy(language, DEFAULT_LANGUAGE);
    
    // Save the reset configuration to NVS
    save_config_to_nvs();
    
    ESP_LOGW(TAG, "✅ Configuration reset to defaults and saved to NVS");
}

// Check if current firmware is valid and mark it as such
static esp_err_t check_and_mark_firmware_valid(void)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(TAG, "❌ Failed to get running partition");
        return ESP_FAIL;
    }
    
    esp_ota_img_states_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to get OTA state: %s", esp_err_to_name(err));
        return err;
    }
    
    ESP_LOGI(TAG, "🔍 Current firmware state: %d", ota_state);
    
    // If firmware is pending verification, mark it as valid
    if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
        ESP_LOGW(TAG, "⚠️ Firmware is pending verification - marking as valid");
        err = esp_ota_mark_app_valid_cancel_rollback();
        if (err == ESP_OK) {
            ESP_LOGI(TAG, "✅ Firmware marked as valid - rollback cancelled");
        } else {
            ESP_LOGE(TAG, "❌ Failed to mark firmware as valid: %s", esp_err_to_name(err));
            return err;
        }
    } else if (ota_state == ESP_OTA_IMG_VALID) {
        ESP_LOGI(TAG, "✅ Firmware already marked as valid");
    } else {
        ESP_LOGW(TAG, "⚠️ Firmware state: %d (unknown)", ota_state);
    }
    
    return ESP_OK;
}

// Perform automatic rollback if current firmware is invalid
static esp_err_t perform_rollback_if_needed(void)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(TAG, "❌ Failed to get running partition");
        return ESP_FAIL;
    }
    
    esp_ota_img_states_t ota_state;
    esp_err_t err = esp_ota_get_state_partition(running, &ota_state);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to get OTA state: %s", esp_err_to_name(err));
        return err;
    }
    
    // Check reset reason to detect crashes
    esp_reset_reason_t reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "🔍 Reset reason: %d", reset_reason);
    
    // If firmware is invalid, perform rollback
    if (ota_state == ESP_OTA_IMG_INVALID) {
        ESP_LOGW(TAG, "🚨 Current firmware is INVALID - performing rollback!");
        
        // Get the previous partition
        const esp_partition_t* prev_partition = esp_ota_get_last_invalid_partition();
        if (prev_partition == NULL) {
            ESP_LOGE(TAG, "❌ No previous partition found for rollback");
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "🔄 Rolling back to partition: %s (offset: 0x%08lx)", 
                 prev_partition->label, prev_partition->address);
        
        // Set the previous partition as boot partition
        err = esp_ota_set_boot_partition(prev_partition);
        log_rollback_info("Set Boot Partition", err);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to set boot partition: %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
        
        ESP_LOGW(TAG, "✅ Rollback completed - rebooting in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
        
        return ESP_OK;
    }
    
    // Check if we're in a crash recovery scenario
    if (reset_reason == ESP_RST_PANIC || reset_reason == ESP_RST_INT_WDT || 
        reset_reason == ESP_RST_TASK_WDT || reset_reason == ESP_RST_WDT) {
        
        ESP_LOGW(TAG, "🚨 System crashed (reason: %d) - checking for rollback...", reset_reason);
        
        // If firmware is PENDING_VERIFY and we crashed, mark it as INVALID
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "🚨 Firmware was PENDING_VERIFY and system crashed - marking as INVALID");
            err = esp_ota_mark_app_invalid_rollback_and_reboot();
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "❌ Failed to mark firmware as invalid: %s", esp_err_to_name(err));
                // Continue with manual rollback
            } else {
                // This will reboot automatically
                return ESP_OK;
            }
        }
        
        // For other crash scenarios, try to rollback to the other partition
        ESP_LOGW(TAG, "🚨 Attempting emergency rollback due to crash...");
        
        // Get the other OTA partition
        const esp_partition_t* other_partition = esp_ota_get_next_update_partition(NULL);
        if (other_partition == NULL) {
            ESP_LOGE(TAG, "❌ No other partition found for emergency rollback");
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "🔄 Emergency rollback to partition: %s (offset: 0x%08lx)", 
                 other_partition->label, other_partition->address);
        
        // Set the other partition as boot partition
        err = esp_ota_set_boot_partition(other_partition);
        log_rollback_info("Emergency Rollback", err);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "❌ Failed to set boot partition: %s", esp_err_to_name(err));
            return ESP_FAIL;
        }
        
        ESP_LOGW(TAG, "✅ Emergency rollback completed - rebooting in 3 seconds...");
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
        
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "✅ Firmware state is valid - no rollback needed");
    return ESP_OK;
}

// Enhanced firmware validation with health checks
static esp_err_t validate_firmware_health(void)
{
    ESP_LOGI(TAG, "🔍 Performing firmware health validation...");
    
    // Check 1: Verify partition integrity
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGE(TAG, "❌ Health check failed: No running partition");
        return ESP_FAIL;
    }
    
    // Check 2: Verify app description
    esp_app_desc_t app_desc;
    const esp_partition_t* partition = esp_ota_get_running_partition();
    esp_err_t err = esp_ota_get_partition_description(partition, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Health check failed: Cannot read app description");
        return ESP_FAIL;
    }
    
    // Check 3: Verify this is a Firminia firmware
    if (strstr(app_desc.project_name, "firminia") == NULL && 
        strstr(app_desc.project_name, "Firminia") == NULL) {
        ESP_LOGW(TAG, "⚠️ Health check warning: Unknown project name: %s", app_desc.project_name);
        // Don't fail here, just warn
    }
    
    // Check 4: Verify version format
    if (strlen(app_desc.version) == 0) {
        ESP_LOGW(TAG, "⚠️ Health check warning: Empty version string");
    }
    
    ESP_LOGI(TAG, "📋 Firmware health check passed:");
    ESP_LOGI(TAG, "  - Project: %s", app_desc.project_name);
    ESP_LOGI(TAG, "  - Version: %s", app_desc.version);
    ESP_LOGI(TAG, "  - Date: %s %s", app_desc.date, app_desc.time);
    
    return ESP_OK;
}

// Boot watchdog functions
static void start_boot_watchdog(void)
{
    boot_start_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    boot_watchdog_active = true;
    last_boot_health_check = boot_start_time;
    ESP_LOGI(TAG, "🐕 Boot watchdog started (timeout: %d ms)", BOOT_WATCHDOG_TIMEOUT_MS);
}

static void stop_boot_watchdog(void)
{
    if (boot_watchdog_active) {
        uint32_t boot_duration = (xTaskGetTickCount() * portTICK_PERIOD_MS) - boot_start_time;
        ESP_LOGI(TAG, "🐕 Boot watchdog stopped - boot completed in %lu ms", boot_duration);
        boot_watchdog_active = false;
    }
}

static void check_boot_watchdog(void)
{
    if (!boot_watchdog_active) {
        return;
    }
    
    uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
    uint32_t boot_duration = current_time - boot_start_time;
    
    // Check if boot timeout exceeded
    if (boot_duration > BOOT_WATCHDOG_TIMEOUT_MS) {
        ESP_LOGE(TAG, "🚨 BOOT WATCHDOG TIMEOUT! Boot took %lu ms (limit: %d ms)", 
                 boot_duration, BOOT_WATCHDOG_TIMEOUT_MS);
        
        // Perform emergency rollback
        ESP_LOGW(TAG, "🔄 Emergency rollback triggered by boot watchdog!");
        
        const esp_partition_t* prev_partition = esp_ota_get_last_invalid_partition();
        if (prev_partition != NULL) {
            ESP_LOGI(TAG, "🔄 Rolling back to previous partition: %s", prev_partition->label);
            esp_err_t err = esp_ota_set_boot_partition(prev_partition);
            log_rollback_info("Emergency Rollback", err);
            if (err == ESP_OK) {
                ESP_LOGW(TAG, "✅ Emergency rollback completed - rebooting...");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
            } else {
                ESP_LOGE(TAG, "❌ Emergency rollback failed: %s", esp_err_to_name(err));
            }
        } else {
            ESP_LOGE(TAG, "❌ No previous partition available for emergency rollback");
        }
        
        boot_watchdog_active = false;
        return;
    }
    
    // Periodic health check during boot
    if (current_time - last_boot_health_check >= BOOT_HEALTH_CHECK_INTERVAL_MS) {
        ESP_LOGI(TAG, "🐕 Boot watchdog check: %lu ms elapsed", boot_duration);
        last_boot_health_check = current_time;
        
        // Check if system is responsive
        // This is a simple check - in a real implementation you might check
        // specific system components or tasks
        if (xTaskGetTickCount() == 0) {
            ESP_LOGW(TAG, "⚠️ Boot watchdog warning: System appears unresponsive");
        }
    }
}

// ============================================================================
// ROLLBACK TEST FUNCTIONS
// ============================================================================

#if ENABLE_ROLLBACK_TESTS

// Test 1: Simulate corrupted firmware (causes crash)
static void test_corrupted_firmware(void)
{
    ESP_LOGW(TAG, "🧪 TEST 1: Simulating corrupted firmware...");
    ESP_LOGW(TAG, "🧪 This will cause a crash and trigger rollback!");
    
    // Give time for logs to be printed
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Simulate corrupted firmware by causing a crash
    ESP_LOGE(TAG, "🧪 TEST: Triggering intentional crash for rollback test");
    
    // Method 1: Null pointer dereference (immediate crash)
    int* null_ptr = NULL;
    *null_ptr = 0xDEADBEEF; // This will cause a crash
    
    // Method 2: Stack overflow (alternative)
    // char stack_overflow[1000000]; // Uncomment to test stack overflow
    
    // Method 3: Infinite loop (alternative)
    // while(1) { /* Infinite loop */ } // Uncomment to test infinite loop
}

// Test 1b: Simulate problematic firmware (safer test)
static void test_problematic_firmware(void)
{
    ESP_LOGW(TAG, "🧪 TEST 1b: Simulating problematic firmware...");
    ESP_LOGW(TAG, "🧪 This will cause a crash and trigger rollback!");
    
    // Give time for logs to be printed
    vTaskDelay(pdMS_TO_TICKS(2000));
    
    // Simulate a crash immediately
    ESP_LOGW(TAG, "🧪 TEST: Triggering immediate crash for rollback test");
    
    // Method 1: Null pointer dereference (causes immediate crash)
    int* null_ptr = NULL;
    *null_ptr = 0xDEADBEEF; // This will cause a crash
    
    // This should not be reached
    ESP_LOGW(TAG, "🧪 TEST: This should not be reached after crash");
}


// Test 2: Simulate firmware validation issues
static void test_firmware_validation(void)
{
    ESP_LOGW(TAG, "🧪 TEST 2: Testing firmware validation...");
    ESP_LOGW(TAG, "🧪 This should show warnings but NOT trigger rollback!");
    
    // Test 2a: Simulate wrong project name
    ESP_LOGW(TAG, "🧪 TEST 2a: Simulating wrong project name...");
    // This is tested in validate_firmware_health() function
    // The function checks for "firminia" or "Firminia" in project name
    
    // Test 2b: Simulate empty version
    ESP_LOGW(TAG, "🧪 TEST 2b: Simulating empty version...");
    // This is tested in validate_firmware_health() function
    // The function checks for empty version string
    
    // Test 2c: Simulate invalid partition
    ESP_LOGW(TAG, "🧪 TEST 2c: Testing partition validation...");
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (running == NULL) {
        ESP_LOGW(TAG, "🧪 TEST: No running partition found (this should trigger health check failure)");
    } else {
        ESP_LOGI(TAG, "🧪 TEST: Running partition found: %s", running->label);
    }
    
    // Test 2d: Simulate app description issues
    ESP_LOGW(TAG, "🧪 TEST 2d: Testing app description validation...");
    esp_app_desc_t app_desc;
    const esp_partition_t* partition = esp_ota_get_running_partition();
    if (partition) {
        esp_err_t err = esp_ota_get_partition_description(partition, &app_desc);
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "🧪 TEST: Failed to read app description (this should trigger health check failure)");
        } else {
            ESP_LOGI(TAG, "🧪 TEST: App description read successfully");
            ESP_LOGI(TAG, "🧪 TEST: Project: %s", app_desc.project_name);
            ESP_LOGI(TAG, "🧪 TEST: Version: %s", app_desc.version);
        }
    }
    
    ESP_LOGI(TAG, "🧪 TEST 2: Firmware validation test completed");
    ESP_LOGI(TAG, "🧪 TEST 2: Check logs for warnings - rollback should NOT occur");
}

// Test runner function
static void run_rollback_tests(void)
{
    ESP_LOGW(TAG, "🧪 ========================================");
    ESP_LOGW(TAG, "🧪 STARTING ROLLBACK TESTS");
    ESP_LOGW(TAG, "🧪 ========================================");
    
    // Test 1: Corrupted firmware (CRASH)
    #if TEST_FIRMWARE_CORRUPTION
    ESP_LOGW(TAG, "🧪 Running Test 1: Corrupted Firmware (CRASH)");
    test_corrupted_firmware();
    #endif
    
    // Test 1b: Problematic firmware (SAFER)
    #if TEST_PROBLEMATIC_FIRMWARE
    ESP_LOGW(TAG, "🧪 Running Test 1b: Problematic Firmware (SAFER)");
    test_problematic_firmware();
    #endif
    
    // Test 2: Firmware validation
    #if TEST_FIRMWARE_VALIDATION
    ESP_LOGW(TAG, "🧪 Running Test 2: Firmware Validation");
    test_firmware_validation();
    #endif
    
    ESP_LOGW(TAG, "🧪 ========================================");
    ESP_LOGW(TAG, "🧪 ROLLBACK TESTS COMPLETED");
    ESP_LOGW(TAG, "🧪 ========================================");
}

// Test activation via button press (alternative method)
static void check_test_activation(void)
{
    // Check if button is pressed during warmup to activate tests
    static bool test_activated = false;
    
    if (!test_activated && gpio_get_level(BUTTON_GPIO) == 1) {
        ESP_LOGW(TAG, "🧪 TEST: Button pressed during warmup - activating rollback tests!");
        test_activated = true;
        
        // Run all tests
        run_rollback_tests();
    }
}

#endif // ENABLE_ROLLBACK_TESTS

 // Callback for BLE (parsing is handled in ble_process_received_data)
 static void on_ble_config_received(const char* json_str)
 {
     ESP_LOGI(TAG, "BLE config received (callback): %s", json_str);
     // Set flag to indicate new configuration was received
     s_config_received_flag = true;
     // Parsing and updating take place in ble_process_received_data.
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

     // Start boot watchdog
     start_boot_watchdog();

     // CRITICAL: Check firmware validity and perform rollback if needed
     ESP_LOGI(TAG, "🔒 Performing firmware security checks...");
     
     // Step 1: Check if rollback is needed
     ESP_LOGI(TAG, "🔍 Step 1: Checking for rollback requirements...");
     esp_err_t rollback_err = perform_rollback_if_needed();
     log_rollback_info("Rollback Check", rollback_err);
     if (rollback_err != ESP_OK) {
         ESP_LOGE(TAG, "❌ Rollback check failed: %s", esp_err_to_name(rollback_err));
         // Continue anyway, but log the error
     }
     
     // Step 2: Validate firmware health
     ESP_LOGI(TAG, "🔍 Step 2: Validating firmware health...");
     esp_err_t health_err = validate_firmware_health();
     log_rollback_info("Health Check", health_err);
     if (health_err != ESP_OK) {
         ESP_LOGW(TAG, "⚠️ Firmware health check failed: %s", esp_err_to_name(health_err));
         // Continue anyway, but log the warning
     }
     
     // Step 3: Mark current firmware as valid (if it's pending verification)
     ESP_LOGI(TAG, "🔍 Step 3: Marking firmware as valid...");
     esp_err_t mark_err = check_and_mark_firmware_valid();
     log_rollback_info("Mark Valid", mark_err);
     if (mark_err != ESP_OK) {
         ESP_LOGW(TAG, "⚠️ Failed to mark firmware as valid: %s", esp_err_to_name(mark_err));
         // Continue anyway, but log the warning
     }
     
     ESP_LOGI(TAG, "✅ Firmware security checks completed");

     // WARMING UP phase
     s_current_state = STATE_WARMING_UP;
     display_manager_update(DISPLAY_STATE_WARMING_UP, 0);

     // Run rollback tests if enabled (after warming up)
     #if ENABLE_ROLLBACK_TESTS
     run_rollback_tests();
     #endif
 
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
     
     // Boot is now complete - stop the watchdog
     stop_boot_watchdog();
 
    while (1) {
        
        /* -- 0. Check boot watchdog (if still active) --- */
        check_boot_watchdog();
        
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
 