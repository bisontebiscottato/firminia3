/*************************************************************
 *                     FIRMINIA 3.4.0                          *
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
 #include "device_config.h"  // Contiene web_server, web_port, web_url, api_token, askmesign_user
 
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
 