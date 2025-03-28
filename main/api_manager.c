#include "api_manager.h"
#include "esp_log.h"
#include "esp_http_client.h"

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
 #include <sys/socket.h>
 #include <netdb.h>
 #include "mbedtls/x509_crt.h"

#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char* TAG = "API_Manager";

int api_manager_check_practices(const char* url, const char* token)
{
    ESP_LOGI(TAG, "Performing HTTPS GET request to URL: %s", url);
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .transport_type = HTTP_TRANSPORT_OVER_SSL, // Using mbedtls for HTTPS
        .timeout_ms = 5000,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return -1;
    }

    if (token && strlen(token) > 0) {
        char auth_header[128];
        snprintf(auth_header, sizeof(auth_header), "Bearer %s", token);
        esp_http_client_set_header(client, "Authorization", auth_header);
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        if (err == MBEDTLS_ERR_X509_CERT_VERIFY_FAILED) {
            ESP_LOGE(TAG, "Certificate verification failed (certificate expired or invalid)");
        }
         else if (err == ESP_ERR_HTTP_CONNECT) {
            ESP_LOGE(TAG, "Failed to connect to server (server not responding)");
        } else {
            ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        }
        esp_http_client_cleanup(client);
        return -1;
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP GET Status = %d", status_code);
    if (status_code != 200) {
        ESP_LOGW(TAG, "Unexpected HTTP status code: %d", status_code);
        esp_http_client_cleanup(client);
        return -1;
    }

    int content_length = esp_http_client_get_content_length(client);
    ESP_LOGI(TAG, "Content length: %d", content_length);
    if (content_length <= 0) {
        ESP_LOGE(TAG, "No content received");
        esp_http_client_cleanup(client);
        return -1;
    }

    char* buffer = malloc(content_length + 1);
    if (buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for response");
        esp_http_client_cleanup(client);
        return -1;
    }

    int read_len = esp_http_client_read(client, buffer, content_length);
    if (read_len <= 0) {
        ESP_LOGE(TAG, "Failed to read response");
        free(buffer);
        esp_http_client_cleanup(client);
        return -1;
    }
    buffer[read_len] = '\0';
    ESP_LOGI(TAG, "Response: %s", buffer);

    int practices_found = 0;
    cJSON* root = cJSON_Parse(buffer);
    if (root == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response");
        free(buffer);
        esp_http_client_cleanup(client);
        return -1;
    }

    cJSON* pending = cJSON_GetObjectItem(root, "pending");
    if (cJSON_IsNumber(pending)) {
        practices_found = pending->valueint;
    } else {
        ESP_LOGE(TAG, "JSON response does not contain a valid 'pending' field");
        practices_found = -1;
    }
    cJSON_Delete(root);
    free(buffer);
    esp_http_client_cleanup(client);

    ESP_LOGI(TAG, "Practices found: %d", practices_found);
    return practices_found;
}
