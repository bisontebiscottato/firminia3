/*************************************************************
 *                     FIRMINIA 3.6.1                          *
 *  File: api_manager.c                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

 #include <stdio.h>
 #include <string.h>
 #include <stdlib.h>
 #include "esp_log.h"
 #include "esp_err.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 #include "mbedtls/platform.h"
 #include "mbedtls/net_sockets.h"
 #include "mbedtls/esp_debug.h"
 #include "mbedtls/ssl.h"
 #include "mbedtls/entropy.h"
 #include "mbedtls/ctr_drbg.h"
 #include "mbedtls/error.h"
 #ifdef CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
     #include "psa/crypto.h"
 #endif
 #include "esp_crt_bundle.h"
 #include "mbedtls/x509_crt.h"
#include "cJSON.h"
#include "esp_http_client.h"
#include "device_config.h"  // Contiene web_server, web_port, web_url, api_token, askmesign_user
#include "ota_manager.h"    // Per ota_version_info_t
 
 static const char *TAG = "API_Manager";
 #define BUFFER_SIZE 8000
 int totalElements = 0;
 
 // Function to extract the JSON body from the HTTP response
 const char *extract_json_body(const char *response) {
     const char *json_start = strstr(response, "\r\n\r\n");
     if (json_start) {
         return json_start + 4;
     }
     return NULL;
 }
 
 int api_manager_check_practices(void)
 {
     int ret;
     size_t offset = 0;
     char *buffer = NULL;
     int practices_found = -1;

    vTaskDelay(pdMS_TO_TICKS(1500));

     // Manual construction of HTTP/1.0 request
     // In questo esempio, usiamo HTTP/1.0 per forzare la chiusura della connessione.
     char request[512];

     #pragma GCC diagnostic push
     #pragma GCC diagnostic ignored "-Wformat-truncation"

     snprintf(request, sizeof(request),
              "GET %s HTTP/1.0\r\n"
              "Host: %s\r\n"
              "User-Agent: ESP-32 S3 1.0\r\n"
              "X-SignToken: %s\r\n"
              "X-SignUser: %s\r\n"
              "\r\n",
              web_url, web_server, api_token, askmesign_user);
     ESP_LOGI(TAG, "HTTP Request:\n%s", request);

     #pragma GCC diagnostic pop

     // Initialization of mbedTLS
     mbedtls_net_context server_fd;
     mbedtls_ssl_context ssl;
     mbedtls_ssl_config conf;
     mbedtls_ctr_drbg_context ctr_drbg;
     mbedtls_entropy_context entropy;
     mbedtls_x509_crt cacert;
     
     mbedtls_net_init(&server_fd);
     mbedtls_ssl_init(&ssl);
     mbedtls_ssl_config_init(&conf);
     mbedtls_ctr_drbg_init(&ctr_drbg);
     mbedtls_entropy_init(&entropy);
     mbedtls_x509_crt_init(&cacert);
 
 #ifdef CONFIG_MBEDTLS_SSL_PROTO_TLS1_3
     psa_status_t status = psa_crypto_init();
     if (status != PSA_SUCCESS) {
         ESP_LOGE(TAG, "PSA crypto init failed: %d", (int) status);
         goto exit;
     }
 #endif
 
     if ((ret = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy, NULL, 0)) != 0) {
         ESP_LOGE(TAG, "mbedtls_ctr_drbg_seed returned -0x%x", -ret);
         goto exit;
     }
 
     if ((ret = mbedtls_ssl_config_defaults(&conf,
                                            MBEDTLS_SSL_IS_CLIENT,
                                            MBEDTLS_SSL_TRANSPORT_STREAM,
                                            MBEDTLS_SSL_PRESET_DEFAULT)) != 0) {
         ESP_LOGE(TAG, "mbedtls_ssl_config_defaults returned -0x%x", -ret);
         goto exit;
     }
 
     mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_REQUIRED);
     mbedtls_ssl_conf_ca_chain(&conf, &cacert, NULL);
     mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);
     mbedtls_ssl_conf_read_timeout(&conf, 5000);
 
     if ((ret = esp_crt_bundle_attach(&conf)) < 0) {
         ESP_LOGE(TAG, "esp_crt_bundle_attach returned -0x%x", -ret);
         goto exit;
     }
 
     if ((ret = mbedtls_ssl_setup(&ssl, &conf)) != 0) {
         ESP_LOGE(TAG, "mbedtls_ssl_setup returned -0x%x", -ret);
         goto exit;
     }
 
     if ((ret = mbedtls_ssl_set_hostname(&ssl, web_server)) != 0) {
         ESP_LOGE(TAG, "mbedtls_ssl_set_hostname returned -0x%x", -ret);
         goto exit;
     }
 
     // Connection to the server
     ESP_LOGI(TAG, "Connecting to %s:%s...", web_server, web_port);
     if ((ret = mbedtls_net_connect(&server_fd, web_server, web_port, MBEDTLS_NET_PROTO_TCP)) != 0) {
         ESP_LOGE(TAG, "mbedtls_net_connect returned -0x%x", -ret);
         goto exit;
     }
 
     mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);
 
     ESP_LOGI(TAG, "Performing the SSL/TLS handshake...");
     while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
         if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
             ESP_LOGE(TAG, "mbedtls_ssl_handshake returned -0x%x", -ret);
             goto exit;
         }
     }
 
     if ((ret = mbedtls_ssl_get_verify_result(&ssl)) != 0) {
         ESP_LOGW(TAG, "Certificate verification failed, flags: 0x%x", ret);
     } else {
         ESP_LOGI(TAG, "Certificate verified.");
     }
 
     // Send the request
     size_t written = 0;
     size_t request_len = strlen(request);
     while (written < request_len) {
         ret = mbedtls_ssl_write(&ssl, (const unsigned char *)request + written, request_len - written);
         if (ret < 0) {
             ESP_LOGE(TAG, "mbedtls_ssl_write returned -0x%x", -ret);
             goto exit;
         }
         written += ret;
     }
     ESP_LOGI(TAG, "Request sent (%d bytes)", written);
 
     // Allocate buffer for the response
     buffer = malloc(BUFFER_SIZE);
     if (buffer == NULL) {
         ESP_LOGE(TAG, "Memory allocation for response failed");
         goto exit;
     }
     memset(buffer, 0, BUFFER_SIZE);
     
     // Reads the HTTP response
     while ((ret = mbedtls_ssl_read(&ssl, (unsigned char *)buffer + offset, BUFFER_SIZE - offset - 1)) > 0) {
         offset += ret;
         if (offset >= BUFFER_SIZE - 1) {
             ESP_LOGE(TAG, "Risposta troppo grande, buffer pieno!");
             break;
         }
     }
     buffer[offset] = '\0';
 
     ESP_LOGI(TAG, "HTTP Response received (%d bytes):\n%s", offset, buffer);
 
     // Extracts JSON body (assuming response contains headers and body separated by "\r\n\r\n")
     const char *json_body = extract_json_body(buffer);
     if (json_body == NULL) {
         ESP_LOGE(TAG, "Failed to extract JSON body from HTTP response");
         practices_found = -1;  // Set error state for JSON extraction failure
         goto exit;
     }
     
     cJSON *json = cJSON_Parse(json_body);
     if (json == NULL) {
         ESP_LOGE(TAG, "Failed to parse JSON body");
         practices_found = -1;  // Set error state for JSON parsing failure
         goto exit;
     }
     
     cJSON *numberElem = cJSON_GetObjectItem(json, "totalElements");
     if (cJSON_IsNumber(numberElem)) {
         totalElements = numberElem->valueint;
         practices_found = totalElements;
         ESP_LOGI(TAG, "Number of Elements: %d", totalElements);
     } else {
         ESP_LOGE(TAG, "JSON does not contain a valid 'totalElements' field");
         practices_found = -1;  // Set error state for invalid JSON
     }
     cJSON_Delete(json);
     
 exit:
     mbedtls_ssl_close_notify(&ssl);
     mbedtls_net_free(&server_fd);
     mbedtls_ssl_free(&ssl);
     mbedtls_ssl_config_free(&conf);
     mbedtls_ctr_drbg_free(&ctr_drbg);
     mbedtls_entropy_free(&entropy);
    mbedtls_x509_crt_free(&cacert);
    if (buffer) {
        free(buffer);
    }
    return practices_found;
}

esp_err_t api_manager_check_firmware_updates(const char* current_version, ota_version_info_t* update_info)
{
    if (current_version == NULL || update_info == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔍 Checking for firmware updates via GitHub API (current: %s)...", current_version);
    
    // Use GitHub API to check for latest release
    char update_url[512];
    snprintf(update_url, sizeof(update_url), 
             "https://api.github.com/repos/bisontebiscottato/firminia3/releases/latest");
    
    ESP_LOGI(TAG, "📡 GitHub API URL: %s", update_url);
    
    // HTTP client configuration for GitHub API
    esp_http_client_config_t config = {
        .url = update_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .buffer_size = 4096,  // GitHub API responses can be larger
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,  // Enable certificate verification
        .skip_cert_common_name_check = false,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "❌ Failed to initialize HTTP client for GitHub API");
        return ESP_ERR_NO_MEM;
    }
    
    // GitHub API headers (no auth needed for public repos)
    esp_http_client_set_header(client, "User-Agent", "Firminia/3.6.1");
    esp_http_client_set_header(client, "Accept", "application/vnd.github.v3+json");
    
    // Allocate response buffer (larger for GitHub API)
    char* buffer = malloc(4096);
    if (buffer == NULL) {
        esp_http_client_cleanup(client);
        ESP_LOGE(TAG, "❌ Failed to allocate response buffer");
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open HTTP connection: %s", esp_err_to_name(err));
        free(buffer);
        esp_http_client_cleanup(client);
        return err;
    }
    
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "📡 Update check response: status=%d, content_length=%d", status_code, content_length);
    
    if (status_code == 200 && content_length > 0 && content_length < 4096) {
        int data_read = esp_http_client_read_response(client, buffer, content_length);
        if (data_read > 0) {
            buffer[data_read] = '\0';
            ESP_LOGI(TAG, "📡 GitHub API response received (%d bytes)", data_read);
            
            // Parse GitHub API JSON response
            cJSON *json = cJSON_Parse(buffer);
            if (json) {
                cJSON *tag_name = cJSON_GetObjectItem(json, "tag_name");
                cJSON *assets = cJSON_GetObjectItem(json, "assets");
                
                if (cJSON_IsString(tag_name) && cJSON_IsArray(assets)) {
                    const char* latest_version = cJSON_GetStringValue(tag_name);
                    ESP_LOGI(TAG, "📋 Latest GitHub release: %s", latest_version);
                    
                    // Simple version comparison (remove 'v' prefix if present)
                    const char* clean_latest = (latest_version[0] == 'v') ? latest_version + 1 : latest_version;
                    const char* clean_current = (current_version[0] == 'v') ? current_version + 1 : current_version;
                    
                    if (strcmp(clean_current, clean_latest) < 0) {
                        // Find firmware binary in assets
                        cJSON *asset;
                        cJSON_ArrayForEach(asset, assets) {
                            cJSON *name = cJSON_GetObjectItem(asset, "name");
                            cJSON *download_url = cJSON_GetObjectItem(asset, "browser_download_url");
                            cJSON *size = cJSON_GetObjectItem(asset, "size");
                            
                            if (cJSON_IsString(name) && cJSON_IsString(download_url) && cJSON_IsNumber(size)) {
                                const char* asset_name = cJSON_GetStringValue(name);
                                
                                // Look for firmware binary
                                if (strstr(asset_name, "firminia3.bin") != NULL) {
                                    // Fill update info structure
                                    const char* download_url_str = cJSON_GetStringValue(download_url);
                                    ESP_LOGI(TAG, "🔗 Found firmware URL: %s", download_url_str);
                                    
                                    // Validate firmware size (should be between 1MB and 10MB for ESP32)
                                    uint32_t firmware_size = (uint32_t)cJSON_GetNumberValue(size);
                                    if (firmware_size < 1024 * 1024 || firmware_size > 10 * 1024 * 1024) {
                                        ESP_LOGW(TAG, "⚠️ Suspicious firmware size: %lu bytes (%.2f MB)", 
                                                firmware_size, firmware_size / (1024.0 * 1024.0));
                                        // Continue anyway but log warning
                                    }
                                    
                                    strncpy(update_info->version, clean_latest, sizeof(update_info->version) - 1);
                                    update_info->version[sizeof(update_info->version) - 1] = '\0';
                                    
                                    strncpy(update_info->url, download_url_str, sizeof(update_info->url) - 1);
                                    update_info->url[sizeof(update_info->url) - 1] = '\0';
                                    
                                    // Generate signature URL (assuming .sig file exists)
                                    snprintf(update_info->signature_url, sizeof(update_info->signature_url),
                                            "https://github.com/bisontebiscottato/firminia3/releases/download/%s/firminia3.sig",
                                            latest_version);
                                    
                                    update_info->size = firmware_size;
                                    
                                    // For now, leave checksum empty (could be added to GitHub release notes)
                                    strcpy(update_info->checksum, "");
                                    
                                    ESP_LOGI(TAG, "✅ Update available: %s → %s (size: %lu bytes, %.2f MB)", 
                                             current_version, update_info->version, update_info->size,
                                             update_info->size / (1024.0 * 1024.0));
                                    
                                    err = ESP_OK;
                                    break;
                                }
                            }
                        }
                        
                        if (err != ESP_OK) {
                            ESP_LOGE(TAG, "❌ firminia3.bin not found in release assets");
                            err = ESP_ERR_NOT_FOUND;
                        }
                    } else {
                        ESP_LOGI(TAG, "ℹ️ No newer version available (%s >= %s)", clean_current, clean_latest);
                        err = ESP_ERR_NOT_FOUND;
                    }
                } else {
                    ESP_LOGE(TAG, "❌ Invalid GitHub API response format");
                    err = ESP_ERR_INVALID_RESPONSE;
                }
                
                if (json) cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "❌ Failed to parse GitHub API JSON response");
                err = ESP_ERR_INVALID_RESPONSE;
            }
        } else {
            ESP_LOGE(TAG, "❌ Failed to read update response");
            err = ESP_ERR_HTTP_INVALID_TRANSPORT;
        }
    } else if (status_code == 404) {
        ESP_LOGI(TAG, "ℹ️ No updates available (404)");
        err = ESP_ERR_NOT_FOUND;
    } else {
        ESP_LOGE(TAG, "❌ Update check failed: HTTP %d", status_code);
        err = ESP_ERR_HTTP_BASE + status_code;
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
    
    return err;
}

// Editor mode: Get user ID from /api/v2/account
esp_err_t api_manager_get_user_id(char* user_id_buffer, size_t buffer_size)
{
    if (!user_id_buffer || buffer_size == 0) {
        ESP_LOGE(TAG, "❌ Invalid buffer parameters");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "🔍 Getting user ID from /api/v2/account...");
    
    // Build account URL
    char account_url[512];
    snprintf(account_url, sizeof(account_url), "https://%s/api/v2/account", web_server);
    
    ESP_LOGI(TAG, "📡 Account API URL: %s", account_url);
    
    // HTTP client configuration
    esp_http_client_config_t config = {
        .url = account_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 30000,
        .buffer_size = 4096,
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = false,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "❌ Failed to initialize HTTP client");
        return ESP_FAIL;
    }
    
    // Set headers
    esp_http_client_set_header(client, "X-SignToken", api_token);
    esp_http_client_set_header(client, "X-SignUser", askmesign_user);
    esp_http_client_set_header(client, "User-Agent", "Firminia/3.6.1");
    esp_http_client_set_header(client, "Accept", "application/json");
    
    // Allocate response buffer
    char* buffer = malloc(4096);
    if (!buffer) {
        ESP_LOGE(TAG, "❌ Failed to allocate response buffer");
        esp_http_client_cleanup(client);
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buffer);
        return err;
    }
    
    // Read response
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "📊 Account API Response - Status: %d, Content Length: %d", status_code, content_length);
    
    if (status_code == 200) {
        int data_read = esp_http_client_read_response(client, buffer, 4095);
        if (data_read > 0) {
            buffer[data_read] = '\0';
            ESP_LOGI(TAG, "📄 Account API Response: %s", buffer);
            
            // Parse JSON to extract idUser
            cJSON *json = cJSON_Parse(buffer);
            if (json) {
                cJSON *id_user = cJSON_GetObjectItem(json, "idUser");
                if (cJSON_IsNumber(id_user)) {
                    snprintf(user_id_buffer, buffer_size, "%d", id_user->valueint);
                    ESP_LOGI(TAG, "✅ User ID extracted: %s", user_id_buffer);
                    cJSON_Delete(json);
                    err = ESP_OK;
                } else {
                    ESP_LOGE(TAG, "❌ idUser not found or not a number in response");
                    cJSON_Delete(json);
                    err = ESP_ERR_NOT_FOUND;
                }
            } else {
                ESP_LOGE(TAG, "❌ Failed to parse JSON response");
                err = ESP_ERR_INVALID_RESPONSE;
            }
        } else {
            ESP_LOGE(TAG, "❌ No data received from account API");
            err = ESP_ERR_INVALID_RESPONSE;
        }
    } else {
        ESP_LOGE(TAG, "❌ Account API error - Status: %d", status_code);
        err = ESP_ERR_HTTP_BASE + status_code;
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
    
    return err;
}

// Editor mode: Check documents created by user (pending signature)
int api_manager_check_editor_documents(const char* user_id)
{
    if (!user_id) {
        ESP_LOGE(TAG, "❌ Invalid user_id parameter");
        return -1;
    }
    
    ESP_LOGI(TAG, "🔍 Checking documents for editor (user ID: %s)...", user_id);
    
    // Build documents URL with query parameters
    char documents_url[512];
    snprintf(documents_url, sizeof(documents_url), 
             "https://%s/api/v2/files/?idUser=%s&page=0&size=1&sort=idFile,desc&status=L", 
             web_server, user_id);
    
    ESP_LOGI(TAG, "📡 Documents API URL: %s", documents_url);
    
    // HTTP client configuration
    esp_http_client_config_t config = {
        .url = documents_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 30000,
        .buffer_size = 65536,  // Increased to 64KB for very large JSON responses
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .skip_cert_common_name_check = false,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "❌ Failed to initialize HTTP client");
        return -1;
    }
    
    // Set headers
    esp_http_client_set_header(client, "X-SignToken", api_token);
    esp_http_client_set_header(client, "X-SignUser", askmesign_user);
    esp_http_client_set_header(client, "User-Agent", "Firminia/3.6.1");
    esp_http_client_set_header(client, "Accept", "application/json");
    
    // Allocate response buffer (64KB for very large JSON responses)
    char* buffer = malloc(65536);
    if (!buffer) {
        ESP_LOGE(TAG, "❌ Failed to allocate response buffer");
        esp_http_client_cleanup(client);
        return -1;
    }
    
    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "❌ Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        free(buffer);
        return -1;
    }
    
    // Read response
    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    
    ESP_LOGI(TAG, "📊 Documents API Response - Status: %d, Content Length: %d", status_code, content_length);
    
    int documents_found = -1;
    
    if (status_code == 200) {
        int data_read = esp_http_client_read_response(client, buffer, 65535);
        if (data_read > 0) {
            buffer[data_read] = '\0';
            ESP_LOGI(TAG, "📄 Documents API Response (%d bytes): %s", data_read, buffer);
            
            // Parse JSON to extract totalElements
            cJSON *json = cJSON_Parse(buffer);
            if (json) {
                cJSON *total_elements = cJSON_GetObjectItem(json, "totalElements");
                if (cJSON_IsNumber(total_elements)) {
                    documents_found = total_elements->valueint;
                    ESP_LOGI(TAG, "✅ Editor documents found: %d", documents_found);
                } else {
                    ESP_LOGE(TAG, "❌ totalElements not found or not a number in response");
                    documents_found = -1;
                }
                cJSON_Delete(json);
            } else {
                ESP_LOGE(TAG, "❌ Failed to parse JSON response");
                documents_found = -1;
            }
        } else {
            ESP_LOGE(TAG, "❌ No data received from documents API");
            documents_found = -1;
        }
    } else {
        ESP_LOGE(TAG, "❌ Documents API error - Status: %d", status_code);
        documents_found = -1;
    }
    
    esp_http_client_close(client);
    esp_http_client_cleanup(client);
    free(buffer);
    
    return documents_found;
}
 