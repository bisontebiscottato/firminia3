#include <string.h>

#include "ble_manager.h"
#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#include "esp_log.h"

static const char* TAG = "BLE_Manager";

// UUID for our configuration characteristic: 0000FF01-0000-1000-8000-00805F9B34FB
static uint8_t config_char_uuid[16] = {
    0xFB, 0x34, 0x9B, 0x5F, 0x80, 0x00, 0x00, 0x80,
    0x00, 0x10, 0x00, 0x00, 0x01, 0xFF, 0x00, 0x00
};

static ble_config_callback_t s_config_callback = NULL;

// Simplified GATT write callback for our config characteristic
static void gatts_write_event_handler(esp_gatts_cb_event_t event, 
                                      esp_gatt_if_t gatts_if,
                                      esp_ble_gatts_cb_param_t *param)
{
    if (param->write.handle && param->write.len > 0) {
        char buf[256] = {0};
        uint16_t len = (param->write.len < sizeof(buf)-1) ? param->write.len : sizeof(buf)-1;
        memcpy(buf, param->write.value, len);
        buf[len] = '\0';
        ESP_LOGI(TAG, "Received BLE write with JSON: %s", buf);
        if (s_config_callback) {
            s_config_callback(buf);
        }
    }
}

// GATT event handler – solo gestione della scrittura in questo esempio
static void gatts_event_handler(esp_gatts_cb_event_t event, 
                                esp_gatt_if_t gatts_if, 
                                esp_ble_gatts_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTS_WRITE_EVT:
            ESP_LOGI(TAG, "GATT Write Event received.");
            gatts_write_event_handler(event, gatts_if, param);
            break;
        default:
            ESP_LOGD(TAG, "Unhandled GATT event: %d", event);
            break;
    }
}

// GAP event handler per advertising
static void gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
            if (param->adv_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "BLE advertising started successfully.");
            } else {
                ESP_LOGE(TAG, "BLE advertising start failed, error code: %d", param->adv_start_cmpl.status);
            }
            break;
        case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "BLE advertising stopped.");
            break;
        default:
            ESP_LOGD(TAG, "Unhandled GAP event: %d", event);
            break;
    }
}

void ble_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing Bluetooth stack using Bluedroid...");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "BT controller initialization failed");
        return;
    }
    if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
        ESP_LOGE(TAG, "BT controller enable failed");
        return;
    }
    if (esp_bluedroid_init() != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid initialization failed");
        return;
    }
    if (esp_bluedroid_enable() != ESP_OK) {
        ESP_LOGE(TAG, "Bluedroid enable failed");
        return;
    }
    // Register GAP and GATTS callbacks
    esp_ble_gap_register_callback(gap_event_handler);
    esp_ble_gatts_register_callback(gatts_event_handler);
    // Register application with a dummy app_id (e.g., 0)
    esp_ble_gatts_app_register(0);
    ESP_LOGI(TAG, "Bluetooth stack initialized successfully.");
}

void ble_manager_start_advertising(void)
{
    ESP_LOGI(TAG, "Starting BLE advertising...");
    // Set advertising parameters
    esp_ble_adv_params_t adv_params = {
        .adv_int_min       = 0x20,
        .adv_int_max       = 0x40,
        .adv_type          = ADV_TYPE_IND,
        .own_addr_type     = BLE_ADDR_TYPE_PUBLIC,
        .channel_map       = ADV_CHNL_ALL,
        .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
    };
    if (esp_ble_gap_start_advertising(&adv_params) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start BLE advertising");
    }
}

void ble_manager_stop_advertising(void)
{
    ESP_LOGI(TAG, "Stopping BLE advertising...");
    if (esp_ble_gap_stop_advertising() != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop BLE advertising");
    }
}

void ble_manager_set_config_callback(ble_config_callback_t callback)
{
    s_config_callback = callback;
}
