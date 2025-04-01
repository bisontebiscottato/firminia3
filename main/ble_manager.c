#include <string.h>
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_log.h"
#include "cJSON.h"

// Inclusioni dei moduli del progetto
#include "ble_manager.h"
#include "device_config.h"      // Configurazione NVS e variabili globali (wifi_ssid, web_server, ecc.)
#include "display_manager.h"    // Per aggiornare lo stato della UI

static const char *TAG = "BLE_Manager";

// --- Buffer per accumulare il JSON ricevuto ---
#define MAX_JSON_SIZE 1024
static char json_buffer[MAX_JSON_SIZE];
static uint16_t json_buffer_index = 0;

// Callback opzionale per notificare eventi di configurazione (se usata dal main_flow)
static ble_config_callback_t s_config_callback = NULL;

// Variabili per gestire la connessione BLE (ora gestite tramite GATTS)
static esp_bd_addr_t current_conn_addr;
static bool ble_is_connected = false;

/* --- DEFINIZIONI PER LA CREAZIONE DEL SERVIZIO GATT --- */
// UUID per la dichiarazione del Primary Service (16-bit standard: 0x2800)
static uint16_t primary_service_uuid = 0x2800;
// UUID per la dichiarazione della caratteristica (16-bit standard: 0x2803)
static uint16_t char_decl_uuid = 0x2803;
// UUID per la dichiarazione del CCCD (16-bit standard: 0x2902)
static uint16_t primary_cccd_uuid = 0x2902;

// UUID del servizio (formato 128-bit). In questo esempio il servizio ha un UUID personalizzato.
static uint8_t service_uuid128[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0xF0, 0xFF, 0x00, 0x00
};
// UUID della caratteristica di configurazione (formato 128-bit)
static uint8_t config_char_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00
};

// Definizione della tabella degli attributi GATT (4 attributi)
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

/**
 * @brief Processa i dati JSON ricevuti via BLE.
 *
 * Il JSON può contenere i campi "ssid", "password", "server", "port", "url", "token" e "user".
 * Se un campo è presente, viene usato per aggiornare la configurazione globale.
 * Successivamente, la configurazione aggiornata viene salvata in NVS e viene notificato l’aggiornamento alla UI.
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

    // Aggiorna le variabili globali di configurazione se il JSON contiene i relativi campi
    cJSON *item = cJSON_GetObjectItem(json, "ssid");
    if (item && item->valuestring) {
        strcpy(wifi_ssid, item->valuestring);
    }
    item = cJSON_GetObjectItem(json, "password");
    if (item && item->valuestring) {
        strcpy(wifi_password, item->valuestring);
    }
    item = cJSON_GetObjectItem(json, "server");
    if (item && item->valuestring) {
        strcpy(web_server, item->valuestring);
    }
    item = cJSON_GetObjectItem(json, "port");
    if (item && item->valuestring) {
        strcpy(web_port, item->valuestring);
    }
    item = cJSON_GetObjectItem(json, "url");
    if (item && item->valuestring) {
        strcpy(web_url, item->valuestring);
    }
    item = cJSON_GetObjectItem(json, "token");
    if (item && item->valuestring) {
        strcpy(api_token, item->valuestring);
    }
    item = cJSON_GetObjectItem(json, "user");
    if (item && item->valuestring) {
        strcpy(askmesign_user, item->valuestring);
    }

    // Salva la nuova configurazione in NVS
    save_config_to_nvs();
    ESP_LOGI(TAG, "✅ Configurazione aggiornata e salvata in NVS!");

    // Aggiorna la UI per notificare l'aggiornamento della configurazione
    display_manager_update(DISPLAY_STATE_CONFIG_UPDATED, 0);

    // Se è stata definita una callback aggiuntiva, notifica il nuovo JSON
    if (s_config_callback) {
        s_config_callback((char *)data);
    }
    
    cJSON_Delete(json);
}

/**
 * @brief Gestisce l'evento di scrittura GATT.
 *
 * I dati ricevuti vengono accumulati in un buffer; quando viene rilevata la presenza
 * del carattere '}' si assume che il JSON sia completo e si processa.
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
        
        // Se il buffer contiene il carattere di chiusura JSON, processa i dati
        if (strchr(json_buffer, '}')) {
            ESP_LOGI(TAG, "✅ JSON completo ricevuto, elaborazione...");
            ble_process_received_data((uint8_t *)json_buffer, json_buffer_index);
            json_buffer_index = 0;
        }
    }
}

/**
 * @brief Gestore degli eventi GATTS.
 *
 * Si occupa della registrazione, creazione della tabella degli attributi, gestione delle scritture e degli eventi di connessione/disconnessione.
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
 * @brief Gestore degli eventi GAP per l’advertising.
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
 * @brief Inizializza lo stack BLE, configura i dati di advertising e crea il servizio GATT.
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
    // Registra le callback GAP e GATTS
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    // Registra l'applicazione BLE (ad es. app_id 0)
    esp_ble_gatts_app_register(0);

    // Imposta il nome del dispositivo PRIMA di configurare i dati di advertising
    esp_ble_gap_set_device_name("ESP32-FIRMINIA");

    // Configura i dati di advertising: includi il nome e il TX power
    esp_ble_adv_data_t adv_data = {
        .set_scan_rsp = false,
        .include_name = true,  // Includi il nome impostato
        .include_txpower = true,
        .appearance = 0x00,
        .flag = (ESP_BLE_ADV_FLAG_GEN_DISC | ESP_BLE_ADV_FLAG_BREDR_NOT_SPT)
    };
    esp_ble_gap_config_adv_data(&adv_data);

    ESP_LOGI(TAG, "✅ Stack Bluetooth inizializzato con successo.");
}

/**
 * @brief Avvia l’advertising BLE.
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
 * @brief Ferma l’advertising BLE.
 */
void ble_manager_stop_advertising(void)
{
    ESP_LOGI(TAG, "Interruzione dell'advertising BLE...");
    if (esp_ble_gap_stop_advertising() != ESP_OK) {
        ESP_LOGE(TAG, "❌ Interruzione dell'advertising BLE fallita");
    }
}

/**
 * @brief Disconnette attivamente il dispositivo BLE connesso, se presente.
 */
void ble_manager_disconnect(void)
{
    if (ble_is_connected) {
        ESP_LOGI(TAG, "Disconnessione del dispositivo BLE attivo.");
        esp_ble_gap_disconnect(current_conn_addr);
    }
}

/**
 * @brief Imposta una callback opzionale da notificare quando viene ricevuta una nuova configurazione via BLE.
 */
void ble_manager_set_config_callback(ble_config_callback_t callback)
{
    s_config_callback = callback;
}
