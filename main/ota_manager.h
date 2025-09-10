/*************************************************************
 *                     FIRMINIA 3.5.3                          *
 *  File: ota_manager.h                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 *  Description: Secure OTA Update Manager                   *
 ************************************************************/

#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_ota_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

// OTA Manager Configuration
#define OTA_RECV_TIMEOUT_MS     120000  // 2 minutes timeout for large firmware
#define OTA_BUFFER_SIZE         4096    // 4KB buffer for OTA data
#define OTA_MAX_RETRIES         3       // Maximum download retries
#define OTA_SIGNATURE_SIZE      256     // RSA-2048 signature size

// OTA Status
typedef enum {
    OTA_STATUS_IDLE = 0,
    OTA_STATUS_CHECKING,
    OTA_STATUS_DOWNLOADING,
    OTA_STATUS_VERIFYING,
    OTA_STATUS_INSTALLING,
    OTA_STATUS_SUCCESS,
    OTA_STATUS_ERROR
} ota_status_t;

// OTA Error Codes
typedef enum {
    OTA_ERROR_NONE = 0,
    OTA_ERROR_HTTP_FAILED,
    OTA_ERROR_DOWNLOAD_FAILED,
    OTA_ERROR_SIGNATURE_INVALID,
    OTA_ERROR_PARTITION_ERROR,
    OTA_ERROR_WRITE_FAILED,
    OTA_ERROR_VERIFY_FAILED,
    OTA_ERROR_ROLLBACK_FAILED
} ota_error_t;

// OTA Progress Callback
typedef void (*ota_progress_callback_t)(int percentage, ota_status_t status, ota_error_t error);

// OTA Version Info
typedef struct {
    char version[32];
    char url[256];
    char signature_url[256];
    uint32_t size;
    char checksum[65]; // SHA256 hex string
} ota_version_info_t;

/**
 * @brief Initialize OTA Manager
 * 
 * @param progress_cb Progress callback function
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_manager_init(ota_progress_callback_t progress_cb);

/**
 * @brief Check for firmware updates
 * 
 * @param current_version Current firmware version
 * @param update_info Output parameter for update information
 * @return esp_err_t ESP_OK if update available, ESP_ERR_NOT_FOUND if no update
 */
esp_err_t ota_check_for_updates(const char* current_version, ota_version_info_t* update_info);

/**
 * @brief Start secure OTA update process
 * 
 * @param update_info Update information from ota_check_for_updates
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_start_update(const ota_version_info_t* update_info);

/**
 * @brief Get current OTA status
 * 
 * @return ota_status_t Current status
 */
ota_status_t ota_get_status(void);

/**
 * @brief Get last OTA error
 * 
 * @return ota_error_t Last error code
 */
ota_error_t ota_get_last_error(void);

/**
 * @brief Cancel ongoing OTA update
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_cancel_update(void);

/**
 * @brief Mark current firmware as valid (after successful boot)
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_mark_firmware_valid(void);

/**
 * @brief Get OTA partition info
 * 
 * @param running_partition Output for running partition
 * @param update_partition Output for update partition
 * @return esp_err_t ESP_OK on success
 */
esp_err_t ota_get_partition_info(const esp_partition_t** running_partition, 
                                 const esp_partition_t** update_partition);

/**
 * @brief Verify firmware signature using RSA
 * 
 * @param firmware_data Firmware binary data
 * @param firmware_size Size of firmware data
 * @param signature RSA signature data
 * @param signature_size Size of signature
 * @return esp_err_t ESP_OK if signature is valid
 */
esp_err_t ota_verify_firmware_signature(const uint8_t* firmware_data, size_t firmware_size,
                                        const uint8_t* signature, size_t signature_size);

#ifdef __cplusplus
}
#endif

#endif // OTA_MANAGER_H
