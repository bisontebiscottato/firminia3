/*************************************************************
 *                     FIRMINIA 3.6.0                          *
 *  File: ota_manager.c                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 *  Description: Secure OTA Update Manager Implementation   *
 ************************************************************/

#include "ota_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include "esp_image_format.h"
#include "esp_crt_bundle.h"
#include "mbedtls/sha256.h"
#include "mbedtls/rsa.h"
#include "mbedtls/pk.h"
#include "mbedtls/md.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "OTA_MANAGER";

// OTA Manager State
typedef struct {
    ota_status_t status;
    ota_error_t last_error;
    ota_progress_callback_t progress_callback;
    esp_https_ota_handle_t ota_handle;
    const esp_partition_t* update_partition;
    const esp_partition_t* running_partition;
    SemaphoreHandle_t mutex;
    TaskHandle_t ota_task_handle;
    int progress_percentage;
} ota_manager_state_t;

static ota_manager_state_t g_ota_state = {0};

// Forward declarations
static void ota_task(void* pvParameter);
static esp_err_t ota_download_firmware(const ota_version_info_t* update_info);
static void ota_set_status(ota_status_t status);
static void ota_set_error(ota_error_t error);
static void ota_notify_progress(int percentage);

esp_err_t ota_manager_init(ota_progress_callback_t progress_cb)
{
    ESP_LOGI(TAG, "🔒 Initializing Secure OTA Manager...");
    
    // Initialize state
    memset(&g_ota_state, 0, sizeof(g_ota_state));
    g_ota_state.status = OTA_STATUS_IDLE;
    g_ota_state.last_error = OTA_ERROR_NONE;
    g_ota_state.progress_callback = progress_cb;
    
    // Create mutex for thread safety
    g_ota_state.mutex = xSemaphoreCreateMutex();
    if (g_ota_state.mutex == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create OTA mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Get partition information
    g_ota_state.running_partition = esp_ota_get_running_partition();
    g_ota_state.update_partition = esp_ota_get_next_update_partition(NULL);
    
    if (g_ota_state.running_partition == NULL || g_ota_state.update_partition == NULL) {
        ESP_LOGE(TAG, "❌ Failed to get OTA partitions");
        return ESP_ERR_NOT_FOUND;
    }
    
    ESP_LOGI(TAG, "✅ Running partition: %s (offset: 0x%08lx, size: %ld)", 
             g_ota_state.running_partition->label,
             g_ota_state.running_partition->address,
             g_ota_state.running_partition->size);
             
    ESP_LOGI(TAG, "✅ Update partition: %s (offset: 0x%08lx, size: %ld)",
             g_ota_state.update_partition->label,
             g_ota_state.update_partition->address,
             g_ota_state.update_partition->size);
    
    ESP_LOGI(TAG, "✅ Secure OTA Manager initialized successfully");
    return ESP_OK;
}

esp_err_t ota_check_for_updates(const char* current_version, ota_version_info_t* update_info)
{
    if (current_version == NULL || update_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔍 Checking for updates (current: %s)...", current_version);
    
    // TODO: Implement actual API call to AskMeSign server
    // For now, return mock data for testing
    
    // Mock update available
    strncpy(update_info->version, "3.6.0", sizeof(update_info->version) - 1);
    strncpy(update_info->url, "https://updates.askme.it/firminia/v3.6.0/firmware.bin", 
            sizeof(update_info->url) - 1);
    strncpy(update_info->signature_url, "https://updates.askme.it/firminia/v3.6.0/firmware.sig", 
            sizeof(update_info->signature_url) - 1);
    strncpy(update_info->checksum, "a1b2c3d4e5f6789012345678901234567890abcdef1234567890abcdef123456", 
            sizeof(update_info->checksum) - 1);
    update_info->size = 1024 * 1024; // 1MB
    
    // Compare versions (simple string comparison for now)
    if (strcmp(current_version, update_info->version) < 0) {
        ESP_LOGI(TAG, "✅ Update available: %s", update_info->version);
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "ℹ️ No updates available");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t ota_start_update(const ota_version_info_t* update_info)
{
    if (update_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        ESP_LOGE(TAG, "❌ Failed to acquire OTA mutex");
        return ESP_ERR_TIMEOUT;
    }
    
    if (g_ota_state.status != OTA_STATUS_IDLE) {
        xSemaphoreGive(g_ota_state.mutex);
        ESP_LOGE(TAG, "❌ OTA update already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Create OTA task
    BaseType_t result = xTaskCreate(ota_task, "ota_task", 8192, (void*)update_info, 5, 
                                    &g_ota_state.ota_task_handle);
    
    if (result != pdPASS) {
        xSemaphoreGive(g_ota_state.mutex);
        ESP_LOGE(TAG, "❌ Failed to create OTA task");
        return ESP_ERR_NO_MEM;
    }
    
    ota_set_status(OTA_STATUS_DOWNLOADING);
    xSemaphoreGive(g_ota_state.mutex);
    
    ESP_LOGI(TAG, "🚀 Starting OTA update to version %s", update_info->version);
    return ESP_OK;
}

static void ota_task(void* pvParameter)
{
    const ota_version_info_t* update_info = (const ota_version_info_t*)pvParameter;
    esp_err_t err = ESP_OK;
    
    ESP_LOGI(TAG, "📥 OTA Task started");
    
    // Step 1: Download firmware
    ota_set_status(OTA_STATUS_DOWNLOADING);
    ota_notify_progress(0);
    
    err = ota_download_firmware(update_info);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Firmware download failed: %s", esp_err_to_name(err));
        ota_set_error(OTA_ERROR_DOWNLOAD_FAILED);
        goto cleanup;
    }
    
    // Step 2: Verify signature (skipped for initial testing)
    ota_set_status(OTA_STATUS_VERIFYING);
    ota_notify_progress(80);
    
    ESP_LOGI(TAG, "ℹ️ Signature verification skipped for testing");
    
    // Step 3: Install firmware
    ota_set_status(OTA_STATUS_INSTALLING);
    ota_notify_progress(90);
    
    err = esp_https_ota_finish(g_ota_state.ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ OTA finish failed: %s", esp_err_to_name(err));
        ota_set_error(OTA_ERROR_WRITE_FAILED);
        goto cleanup;
    }
    
    ota_set_status(OTA_STATUS_SUCCESS);
    ota_notify_progress(100);
    
    ESP_LOGI(TAG, "✅ OTA update completed successfully! Rebooting...");
    vTaskDelay(pdMS_TO_TICKS(2000)); // Give time for UI update
    esp_restart();
    
cleanup:
    ota_set_status(OTA_STATUS_ERROR);
    g_ota_state.ota_task_handle = NULL;
    vTaskDelete(NULL);
}

static esp_err_t ota_download_firmware(const ota_version_info_t* update_info)
{
    ESP_LOGI(TAG, "📥 Downloading firmware from: %s", update_info->url);
    
    esp_http_client_config_t config = {
        .url = update_info->url,
        .timeout_ms = OTA_RECV_TIMEOUT_MS,
        .keep_alive_enable = true,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Enable certificate verification
        .skip_cert_common_name_check = false,
        .buffer_size = 8192,        // Increased buffer for large firmware files
        .buffer_size_tx = 2048,     // Increased TX buffer
    };
    
    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };
    
    esp_err_t err = esp_https_ota_begin(&ota_config, &g_ota_state.ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ HTTPS OTA begin failed: %s", esp_err_to_name(err));
        return err;
    }
    
    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(g_ota_state.ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to get image description: %s", esp_err_to_name(err));
        esp_https_ota_abort(g_ota_state.ota_handle);
        return err;
    }
    
    ESP_LOGI(TAG, "📋 New firmware info:");
    ESP_LOGI(TAG, "  - Version: %s", app_desc.version);
    ESP_LOGI(TAG, "  - Project: %s", app_desc.project_name);
    ESP_LOGI(TAG, "  - Date: %s %s", app_desc.date, app_desc.time);
    
    // Download with progress updates
    int stall_counter = 0;
    int last_data_read = 0;
    
    while (1) {
        err = esp_https_ota_perform(g_ota_state.ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Update progress
        int data_read = esp_https_ota_get_image_len_read(g_ota_state.ota_handle);
        int progress = (data_read * 70) / update_info->size; // 0-70% for download
        
        // Debug log every 10 iterations to monitor download
        static int debug_counter = 0;
        if (++debug_counter % 10 == 0) {
            ESP_LOGI(TAG, "📊 Download progress: %d bytes / %lu bytes (%d%%)", 
                     data_read, update_info->size, progress);
        }
        
        // Check for download stall
        if (data_read == last_data_read) {
            stall_counter++;
            if (stall_counter > 100) { // 5 seconds at 50ms intervals
                ESP_LOGW(TAG, "⚠️ Download appears stalled, continuing...");
                stall_counter = 0;
            }
        } else {
            stall_counter = 0;
            last_data_read = data_read;
        }
        
        // Update progress only occasionally to reduce SPI conflicts
        static int last_progress = -1;
        static uint32_t last_update_time = 0;
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
        
        // Update every 5% or every 2 seconds (LVGL is suspended, so safe now)
        if (progress != last_progress && 
            (progress - last_progress >= 5 || current_time - last_update_time > 2000)) {
            ota_notify_progress(progress);
            last_progress = progress;
            last_update_time = current_time;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50)); // Minimal delay since LVGL is suspended
    }
    
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ HTTPS OTA perform failed: %s", esp_err_to_name(err));
        esp_https_ota_abort(g_ota_state.ota_handle);
        return err;
    }
    
    ESP_LOGI(TAG, "✅ Firmware download completed");
    return ESP_OK;
}

static void ota_set_status(ota_status_t status)
{
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_ota_state.status = status;
        xSemaphoreGive(g_ota_state.mutex);
    }
}

static void ota_set_error(ota_error_t error)
{
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        g_ota_state.last_error = error;
        g_ota_state.status = OTA_STATUS_ERROR;
        xSemaphoreGive(g_ota_state.mutex);
    }
}

static void ota_notify_progress(int percentage)
{
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        g_ota_state.progress_percentage = percentage;
        if (g_ota_state.progress_callback) {
            g_ota_state.progress_callback(percentage, g_ota_state.status, g_ota_state.last_error);
        }
        xSemaphoreGive(g_ota_state.mutex);
    }
}

ota_status_t ota_get_status(void)
{
    ota_status_t status = OTA_STATUS_IDLE;
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        status = g_ota_state.status;
        xSemaphoreGive(g_ota_state.mutex);
    }
    return status;
}

ota_error_t ota_get_last_error(void)
{
    ota_error_t error = OTA_ERROR_NONE;
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) == pdTRUE) {
        error = g_ota_state.last_error;
        xSemaphoreGive(g_ota_state.mutex);
    }
    return error;
}

esp_err_t ota_cancel_update(void)
{
    if (xSemaphoreTake(g_ota_state.mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    if (g_ota_state.ota_task_handle != NULL) {
        vTaskDelete(g_ota_state.ota_task_handle);
        g_ota_state.ota_task_handle = NULL;
    }
    
    if (g_ota_state.ota_handle) {
        esp_https_ota_abort(g_ota_state.ota_handle);
        g_ota_state.ota_handle = NULL;
    }
    
    g_ota_state.status = OTA_STATUS_IDLE;
    g_ota_state.last_error = OTA_ERROR_NONE;
    
    xSemaphoreGive(g_ota_state.mutex);
    
    ESP_LOGI(TAG, "🚫 OTA update cancelled");
    return ESP_OK;
}

esp_err_t ota_mark_firmware_valid(void)
{
    const esp_partition_t* running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
            if (err == ESP_OK) {
                ESP_LOGI(TAG, "✅ Firmware marked as valid");
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "❌ Failed to mark firmware as valid: %s", esp_err_to_name(err));
                return err;
            }
        }
    }
    
    ESP_LOGI(TAG, "ℹ️ Firmware already marked as valid");
    return ESP_OK;
}

esp_err_t ota_get_partition_info(const esp_partition_t** running_partition, 
                                 const esp_partition_t** update_partition)
{
    if (running_partition) {
        *running_partition = g_ota_state.running_partition;
    }
    if (update_partition) {
        *update_partition = g_ota_state.update_partition;
    }
    return ESP_OK;
}
