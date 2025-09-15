/*************************************************************
 *                     FIRMINIA 3.6.0                          *
 *  File: api_manager.h                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

#ifndef API_MANAGER_H
#define API_MANAGER_H

#include "ota_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

// Returns the number of practices found (or -1 on error)
int api_manager_check_practices(void);

// Editor mode: Get user ID from /api/v2/account
// Returns ESP_OK on success, error code on failure
esp_err_t api_manager_get_user_id(char* user_id_buffer, size_t buffer_size);

// Editor mode: Check documents created by user (pending signature)
// Returns the number of documents found (or -1 on error)
int api_manager_check_editor_documents(const char* user_id);

// Check for firmware updates on AskMeSign server
// Returns ESP_OK if update available, ESP_ERR_NOT_FOUND if no update
esp_err_t api_manager_check_firmware_updates(const char* current_version, ota_version_info_t* update_info);

#ifdef __cplusplus
}
#endif

#endif // API_MANAGER_H
