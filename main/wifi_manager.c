/*************************************************************
 *                     FIRMINIA 3.5.4                          *
 *  File: wifi_manager.c                                     *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 *************************************************************/

 #include "wifi_manager.h"
 #include "esp_wifi.h"
 #include "esp_netif.h"
 #include "esp_event.h"
 #include "esp_log.h"
 #include "esp_netif_types.h"
 #include <string.h>
 
 static const char* TAG = "WiFi_Manager";
 static bool s_connected = false;
 static EventGroupHandle_t s_wifi_event_group;
 #define WIFI_CONNECTED_BIT BIT0
 
 static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
 {
     if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
         ESP_LOGI(TAG, "WIFI_EVENT_STA_START");
     } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
         ESP_LOGW(TAG, "WIFI_EVENT_STA_DISCONNECTED");
         s_connected = false;
         xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
     } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
         ESP_LOGI(TAG, "IP_EVENT_STA_GOT_IP");
         s_connected = true;
         xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
     }
 }
 
 void wifi_manager_init(void)
 {
     ESP_LOGI(TAG, "Initializing Wi-Fi...");
     s_wifi_event_group = xEventGroupCreate(); 
     esp_netif_init();
     esp_event_loop_create_default();
     // Create and store the Wi-Fi station handle
     esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
     // Set custom hostname (name displayed on the network)
     esp_netif_set_hostname(sta_netif, "Firminia3-Lascaux");
 
     wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
     esp_wifi_init(&cfg);
     esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL, NULL);
     esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL, NULL);
     esp_wifi_set_mode(WIFI_MODE_STA);
     // Recommended Wi-Fi optimizations
     esp_wifi_set_max_tx_power(84);  // Maximum power
     esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B | WIFI_PROTOCOL_11G | WIFI_PROTOCOL_11N);
         
     esp_wifi_start();
     ESP_LOGI(TAG, "Wi-Fi initialization completed.");
 }
 
 
 bool wifi_manager_connect(const char* ssid, const char* password)
 {
     ESP_LOGI(TAG, "Attempting to connect to SSID: %s", ssid);
     wifi_config_t wifi_config = {0};
     strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
     strncpy((char*)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
     esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
     xEventGroupClearBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
     esp_wifi_connect();
     ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
     EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE, pdMS_TO_TICKS(5000));
     if (bits & WIFI_CONNECTED_BIT) {
         ESP_LOGI(TAG, "Wi-Fi connected successfully!");
         return true;
     } else {
         ESP_LOGW(TAG, "Wi-Fi connection timed out or failed.");
         return false;
     }
 }
 
 bool wifi_manager_is_connected(void)
 {
     return s_connected;
 }
 