/*************************************************************
 *                     FIRMINIA 3.4.0                          *
 *  File: ble_manager.c                                      *
 *  Author: Andrea Mancini     E-mail: biso@biso.it          *
 ************************************************************/

 #include <string.h>
 #include <ctype.h>
 #include "esp_bt.h"
 #include "esp_gap_ble_api.h"
 #include "esp_gatts_api.h"
 #include "esp_bt_main.h"
 #include "esp_bt_device.h"
 #include "esp_log.h"
 #include "cJSON.h"
 #include "esp_random.h"
 #include "freertos/FreeRTOS.h"
 #include "freertos/task.h"
 
 // Project module inclusions
 #include "ble_manager.h"
 #include "device_config.h"
 #include "display_manager.h"
 #include "translations.h"    
 
 static const char *TAG = "BLE_Manager";
 
 // --- Buffer for JSON ---
 #define MAX_JSON_SIZE 2048
 static char json_buffer[MAX_JSON_SIZE];
 static uint16_t json_buffer_index = 0;
 
 // Optional callback to notify configuration events (if used by main_flow)
 static ble_config_callback_t s_config_callback = NULL;
 
 // Variables to manage BLE connection (now managed via GATTS)
 static esp_bd_addr_t current_conn_addr;
 static bool ble_is_connected = false;
 
 // Device name buffer
 static char device_name_buffer[32] = {0};
 
/* --- DEFINITIONS FOR GATT SERVICE CREATION --- */
// UUID for Primary Service declaration (standard 16-bit: 0x2800)
static uint16_t primary_service_uuid = 0x2800;
// UUID for Characteristic declaration (standard 16-bit: 0x2803)
static uint16_t char_decl_uuid = 0x2803;
// UUID for CCCD declaration (standard 16-bit: 0x2902)
static uint16_t primary_cccd_uuid = 0x2902;

// UUID of the service (128-bit format). In this example, the service has a custom UUID.
static uint8_t service_uuid128[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF0, 0xFF, 0x00, 0x00
};
// UUID of the configuration characteristic (128-bit format)
static uint8_t config_char_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00
};

 
 // Definition of GATT attribute table (4 attributes)
 // 0: Service Declaration, 1: Characteristic Declaration,
 // 2: Characteristic Value, 3: Client Characteristic Configuration Descriptor (CCCD)
 static esp_gatts_attr_db_t gatt_db[4] =
 {
     // [0] Primary Service Declaration
     [0] = {
         { ESP_GATT_AUTO_RSP },
         {
             ESP_UUID_LEN_16, (uint8_t *)&primary_service_uuid,
             ESP_GATT_PERM_READ,
             sizeof(service_uuid128), sizeof(service_uuid128), service_uuid128
         }
     },
     // [1] Characteristic Declaration
     [1] = {
         { ESP_GATT_AUTO_RSP },
         {
             ESP_UUID_LEN_16, (uint8_t *)&char_decl_uuid,
             ESP_GATT_PERM_READ,
             sizeof(uint8_t), sizeof(uint8_t),
             (uint8_t *)&(uint8_t){ ESP_GATT_CHAR_PROP_BIT_READ | ESP_GATT_CHAR_PROP_BIT_WRITE | ESP_GATT_CHAR_PROP_BIT_NOTIFY }
         }
     },
     // [2] Characteristic Value (configuration JSON)
     [2] = {
         { ESP_GATT_AUTO_RSP },
         {
             ESP_UUID_LEN_128, config_char_uuid,
             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
             MAX_JSON_SIZE, 0, NULL
         }
     },
     // [3] Client Characteristic Configuration Descriptor (CCCD)
     [3] = {
         { ESP_GATT_AUTO_RSP },
         {
             ESP_UUID_LEN_16, (uint8_t *)&primary_cccd_uuid,
             ESP_GATT_PERM_READ | ESP_GATT_PERM_WRITE,
             sizeof(uint16_t), sizeof(uint16_t), (uint8_t *)&(uint16_t){0}
         }
     }
 };
 
 
 // SSID: not empty, minimum length (e.g. 1 character)
 static bool validate_ssid(const char *ssid) {
     return ssid && strlen(ssid) >= 1;
 }
 
 // Password: no particular checks, accepts empty strings as well
 static bool validate_password(const char *password) {
     return password != NULL;
 }
 
 // Server: must contain at least one dot
 static bool validate_server(const char *server) {
     return server && strchr(server, '.') != NULL;
 }
 
 // Port: must consist solely of digits, and the numeric value must be between 1 and 65535
 static bool validate_port(const char *port_str) {
     if (!port_str || strlen(port_str) == 0)
         return false;
     for (size_t i = 0; i < strlen(port_str); i++) {
         if (!isdigit((unsigned char)port_str[i])) {
             return false;
         }
     }
     int port = atoi(port_str);
     return (port >= 1 && port <= 65535);
 }
 
 // URL: must start with "https://"
 static bool validate_url(const char *url) {
     if (!url)
         return false;
     return (strncmp(url, "https://", 8) == 0);
 }
 
 // Token: must contain only numbers and letters
 static bool validate_token(const char *token) {
     if (!token)
         return false;
     for (size_t i = 0; i < strlen(token); i++) {
         if (!isalnum((unsigned char)token[i])) {
             return false;
         }
     }
     return true;
 }
 
 // User: no particular checks
 static bool validate_user(const char *user) {
     return user != NULL;
 }
 
// Interval: must consist solely of digits, and the numeric value must be between 10000 and 9000000
 static bool validate_interval(const char *interval_str) {
     if (!interval_str || strlen(interval_str) == 0)
         return false;
     for (size_t i = 0; i < strlen(interval_str); i++) {
         if (!isdigit((unsigned char)interval_str[i])) {
             return false;
         }
     }
     int interval = atoi(interval_str);
     return (interval >= 10000 && interval <= 9000000);
 }

 static bool validate_language(const char *language_str) {
     if (!language_str || strlen(language_str) == 0)
         return false;
     for (size_t i = 0; i < strlen(language_str); i++) {
         if (!isdigit((unsigned char)language_str[i])) {
             return false;
         }
     }
     int lang = atoi(language_str);
     return (lang >= 0 && lang < LANGUAGE_COUNT);
 }

/**
 * @brief Generate a random device name with format "FIRMINIA-XXX"
 * where XXX is a random number between 000 and 999.
 */
static void generate_random_device_name(void)
 {
     // Generate random number between 0 and 999
     uint32_t random_num = esp_random() % 1000;
     
     // Format the device name
     snprintf(device_name_buffer, sizeof(device_name_buffer), 
              "FIRMINIA-%03lu", (unsigned long)random_num);
     
     ESP_LOGI(TAG, "Generated device name: %s", device_name_buffer);
 }

 /**
  * @brief JSON values processing function.
  *
  */
 static void ble_process_received_data(uint8_t *data, uint16_t length)
 {
     if (length >= MAX_JSON_SIZE) {
         ESP_LOGE(TAG, "❌ Errore: Dati ricevuti troppo lunghi! (%d bytes, max %d bytes)", length, MAX_JSON_SIZE);
         return;
     }
     
     data[length] = '\0';
     ESP_LOGI(TAG, "📥 Ricevuto JSON: %s", (char *)data);
 
     cJSON *json = cJSON_Parse((char *)data);
     if (!json) {
         ESP_LOGE(TAG, "❌ Errore nel parsing del JSON!");
         return;
     }
 
     // Extract JSON fields
     cJSON *ssid_item = cJSON_GetObjectItemCaseSensitive(json, "ssid");
     cJSON *password_item = cJSON_GetObjectItemCaseSensitive(json, "password");
     cJSON *server_item = cJSON_GetObjectItemCaseSensitive(json, "server");
     cJSON *port_item = cJSON_GetObjectItemCaseSensitive(json, "port");
     cJSON *url_item = cJSON_GetObjectItemCaseSensitive(json, "url");
     cJSON *token_item = cJSON_GetObjectItemCaseSensitive(json, "token");
     cJSON *user_item = cJSON_GetObjectItemCaseSensitive(json, "user");
     cJSON *interval_item = cJSON_GetObjectItemCaseSensitive(json, "interval");
     cJSON *language_item = cJSON_GetObjectItemCaseSensitive(json, "language");
 
     bool valid = true;
 
     if (!cJSON_IsString(ssid_item) || !validate_ssid(ssid_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'ssid' mancante o non valido");
         valid = false;
     }
     if (!cJSON_IsString(password_item) || !validate_password(password_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'password' mancante (anche vuoto va bene, ma deve essere presente)");
         valid = false;
     }
     if (!cJSON_IsString(server_item) || !validate_server(server_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'server' mancante o non valido");
         valid = false;
     }
     if (!cJSON_IsString(port_item) || !validate_port(port_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'port' mancante o non valido");
         valid = false;
     }
     if (!cJSON_IsString(url_item) || !validate_url(url_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'url' mancante o non valido (deve iniziare con \"https://\")");
         valid = false;
     }
     if (!cJSON_IsString(token_item) || !validate_token(token_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'token' mancante o non valido (solo numeri e lettere)");
         valid = false;
     }
     if (!cJSON_IsString(user_item) || !validate_user(user_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'user' mancante");
         valid = false;
     }
    if (!cJSON_IsString(interval_item) || !validate_interval(interval_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'interval' mancante");
         valid = false;
     }
    if (!cJSON_IsString(language_item) || !validate_language(language_item->valuestring)) {
         ESP_LOGE(TAG, "❌ Campo 'language' mancante o non valido (0=EN, 1=IT, 2=FR, 3=ES)");
         valid = false;
     }
     if (!valid) {
         ESP_LOGE(TAG, "❌ JSON non valido. Ignoro la configurazione.");
         cJSON_Delete(json);
         return;
     }
 
     // Update global configuration variables, if present in JSON
     strcpy(wifi_ssid, ssid_item->valuestring);
     strcpy(wifi_password, password_item->valuestring);
     strcpy(web_server, server_item->valuestring);
     strcpy(web_port, port_item->valuestring);
     strcpy(web_url, url_item->valuestring);
     strcpy(api_token, token_item->valuestring);
     strcpy(askmesign_user, user_item->valuestring);
     strcpy(api_interval_ms, interval_item->valuestring);
     strcpy(language, language_item->valuestring);

     // Save the updated configuration to NVS
     save_config_to_nvs();
     ESP_LOGI(TAG, "✅ Configurazione aggiornata e salvata in NVS!");
 
     // Update language setting
     language_t new_lang = (language_t)atoi(language);
     if (is_valid_language(new_lang)) {
         set_current_language(new_lang);
         ESP_LOGI(TAG, "Language updated to: %s", get_language_name(new_lang));
     }
 
     // Update UI state
     display_manager_update(DISPLAY_STATE_CONFIG_UPDATED, 0);
 
     // If a additional callback is set, call it with the JSON data
     if (s_config_callback) {
         s_config_callback((char *)data);
     }
     
     cJSON_Delete(json);
 }
 
 
 /**
  * @brief GAP response manager for BLE events.
  *
  */
 static void gatts_write_event_handler(esp_gatts_cb_event_t event, 
                                       esp_gatt_if_t gatts_if,
                                       esp_ble_gatts_cb_param_t *param)
 {
     if (param->write.handle && param->write.len > 0) {
         if (json_buffer_index + param->write.len >= MAX_JSON_SIZE) {
             ESP_LOGE(TAG, "❌ Errore: Buffer JSON pieno!");
             json_buffer_index = 0;
             return;
         }
         memcpy(&json_buffer[json_buffer_index], param->write.value, param->write.len);
         json_buffer_index += param->write.len;
         json_buffer[json_buffer_index] = '\0';
         
         ESP_LOGI(TAG, "📥 Buffer JSON attuale: %s", json_buffer);
         
         // If the buffer contains a closing brace, process the JSON
         if (strchr(json_buffer, '}')) {
             ESP_LOGI(TAG, "✅ JSON completo ricevuto, elaborazione...");
             ble_process_received_data((uint8_t *)json_buffer, json_buffer_index);
             json_buffer_index = 0;
         }
     }
 }
 
 /**
  * @brief GAP manager for BLE events.
  */
 static void gatts_event_handler(esp_gatts_cb_event_t event, 
                                 esp_gatt_if_t gatts_if, 
                                 esp_ble_gatts_cb_param_t *param)
 {
     switch (event) {
         case ESP_GATTS_REG_EVT:
             ESP_LOGI(TAG, "Registrazione GATTS completata, gatts_if: %d", gatts_if);
             esp_ble_gatts_create_attr_tab(gatt_db, gatts_if, 4, 0);
             break;
         case ESP_GATTS_CREAT_ATTR_TAB_EVT:
             if (param->add_attr_tab.status != ESP_GATT_OK) {
                 ESP_LOGE(TAG, "Creazione tabella attributi fallita, status: %d", param->add_attr_tab.status);
             } else if (param->add_attr_tab.num_handle != 4) {
                 ESP_LOGE(TAG, "Numero di handle creati non corrisponde: %d/4", param->add_attr_tab.num_handle);
             } else {
                 ESP_LOGI(TAG, "Tabella attributi creata con successo.");
                 esp_ble_gatts_start_service(param->add_attr_tab.handles[0]);
             }
             break;
         case ESP_GATTS_CONNECT_EVT:
             ESP_LOGI(TAG, "BLE device connesso (GATTS)!");
             memcpy(current_conn_addr, param->connect.remote_bda, ESP_BD_ADDR_LEN);
             ble_is_connected = true;
             break;
         case ESP_GATTS_DISCONNECT_EVT:
             ESP_LOGI(TAG, "BLE device disconnesso (GATTS)!");
             ble_is_connected = false;
             break;
         case ESP_GATTS_WRITE_EVT:
             ESP_LOGI(TAG, "GATT Write Event ricevuto.");
             gatts_write_event_handler(event, gatts_if, param);
             break;
         default:
             ESP_LOGD(TAG, "Evento GATTS non gestito: %d", event);
             break;
     }
 }
 
 /**
  * @brief GAP manager for BLE advertising events.
  */
 static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
 {
     switch (event) {
         case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
             if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                 ESP_LOGI(TAG, "✅ BLE advertising avviato con successo.");
             } else {
                 ESP_LOGE(TAG, "❌ Errore nell'avvio dell'advertising, codice: %d", param->adv_start_cmpl.status);
             }
             break;
         case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
             ESP_LOGI(TAG, "BLE advertising interrotto.");
             break;
         default:
             ESP_LOGD(TAG, "Evento GAP non gestito: %d", event);
             break;
     }
 }
 
 /**
  * @brief Init the Bluetooth stack (Bluedroid). 
  */
 void ble_manager_init(void)
 {
     ESP_LOGI(TAG, "Inizializzazione dello stack Bluetooth (Bluedroid)...");
     esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
     if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
         ESP_LOGE(TAG, "Inizializzazione BT controller fallita");
         return;
     }
     if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
         ESP_LOGE(TAG, "Abilitazione BT controller fallita");
         return;
     }
     if (esp_bluedroid_init() != ESP_OK) {
         ESP_LOGE(TAG, "Inizializzazione Bluedroid fallita");
         return;
     }
     if (esp_bluedroid_enable() != ESP_OK) {
         ESP_LOGE(TAG, "Abilitazione Bluedroid fallita");
         return;
     }
     // Record the Bluetooth device address
     esp_ble_gap_register_callback(gap_event_handler);
     esp_ble_gatts_register_callback(gatts_event_handler);

     // Record the application ID
     esp_ble_gatts_app_register(0);
 
     // Generate a random device name
     generate_random_device_name();
     
     // Set the name of the device, before send the data
     esp_ble_gap_set_device_name(device_name_buffer);
 
     // Configure the advertising data
     esp_ble_adv_data_t adv_data = {
         .set_scan_rsp = false,
         .include_name = true,  // Include the device name in the advertising data
         .include_txpower = true,
         .appearance = 0x00,
         .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)
     };
     esp_ble_gap_config_adv_data(&adv_data);
 
     ESP_LOGI(TAG, "✅ Stack Bluetooth inizializzato con successo.");
 }
 
 /**
  * @brief Start BLE Advertising.
  */
 void ble_manager_start_advertising(void)
 {
     ESP_LOGI(TAG, "Avvio dell'advertising BLE...");
     esp_ble_adv_params_t adv_params = {
         .adv_int_min       = 0x20,
         .adv_int_max       = 0x40,
         .adv_type          = ADV_TYPE_IND,
         .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
         .channel_map       = ADV_CHNL_ALL,
         .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
     };
     if (esp_ble_gap_start_advertising(&adv_params) != ESP_OK) {
         ESP_LOGE(TAG, "❌ Avvio dell'advertising BLE fallito");
     }
 }
 
 /**
  * @brief Stop BLE Advertising.
  */
 void ble_manager_stop_advertising(void)
 {
     ESP_LOGI(TAG, "Interruzione dell'advertising BLE...");
     if (esp_ble_gap_stop_advertising() != ESP_OK) {
         ESP_LOGE(TAG, "❌ Interruzione dell'advertising BLE fallita");
     }
 }
 
 /**
  * @brief Active disconnect from the current BLE device.
  */
 void ble_manager_disconnect(void)
 {
     if (ble_is_connected) {
         ESP_LOGI(TAG, "Disconnessione del dispositivo BLE attivo.");
         esp_ble_gap_disconnect(current_conn_addr);
         
         // Add a small delay to ensure disconnection is processed
         vTaskDelay(pdMS_TO_TICKS(50));
     }
 }
 
 /**
  * @brief Set a callback function to be called when the configuration is updated via BLE.
  */
 void ble_manager_set_config_callback(ble_config_callback_t callback)
 {
     s_config_callback = callback;
 }

/**
 * @brief Get the current BLE device name.
 *
 * @return Pointer to the device name string.
 */
const char* ble_manager_get_device_name(void)
 {
     return device_name_buffer;
 }

/**
 * @brief Set the BLE device name.
 *
 * @param name The new device name.
 * @return true if successful, false otherwise.
 */
bool ble_manager_set_device_name(const char* name)
 {
     if (name == NULL || strlen(name) == 0) {
         return false;
     }
     
     // Check if the name fits in our buffer
     if (strlen(name) >= sizeof(device_name_buffer)) {
         ESP_LOGE(TAG, "Device name too long: %s", name);
         return false;
     }
     
     // Update the device name in the BLE stack
     esp_err_t ret = esp_ble_gap_set_device_name(name);
     if (ret != ESP_OK) {
         ESP_LOGE(TAG, "Failed to set device name: %s", esp_err_to_name(ret));
         return false;
     }
     
     // Update our local buffer
     strcpy(device_name_buffer, name);
     
     ESP_LOGI(TAG, "Device name set to: %s", name);
     return true;
 }
 