#include "display_manager.h"
#include "esp_log.h"
// Include your specific LCD driver header if needed, e.g. esp_lcd_gc9a01.h

static const char* TAG = "Display_Manager";

void display_manager_init(void)
{
    ESP_LOGI(TAG, "Initializing display...");
    // Initialize your display here (esp_lcd, LVGL, etc.)
}

void display_manager_update(display_state_t state, int practices_count)
{
    switch (state) {
        case DISPLAY_STATE_WARMING_UP:
            ESP_LOGI(TAG, "[DISPLAY] Warming up...");
            break;
        case DISPLAY_STATE_BLE_ADVERTISING:
            ESP_LOGI(TAG, "[DISPLAY] BLE Advertising - waiting for config...");
            break;
        case DISPLAY_STATE_CONFIG_UPDATED:
            ESP_LOGI(TAG, "[DISPLAY] Configuration updated!");
            break;
        case DISPLAY_STATE_WIFI_CONNECTING:
            ESP_LOGI(TAG, "[DISPLAY] Connecting to Wi-Fi...");
            break;
        case DISPLAY_STATE_CHECKING_API:
            ESP_LOGI(TAG, "[DISPLAY] Checking API...");
            break;
        case DISPLAY_STATE_SHOW_PRACTICES:
            ESP_LOGI(TAG, "[DISPLAY] %d practices to sign!", practices_count);
            break;
        case DISPLAY_STATE_NO_PRACTICES:
            ESP_LOGI(TAG, "[DISPLAY] No practices to sign.");
            break;
        case DISPLAY_STATE_NO_WIFI_SLEEPING:
            ESP_LOGI(TAG, "[DISPLAY] No Wi-Fi - sleeping...");
            break;
        case DISPLAY_STATE_API_ERROR:
            ESP_LOGI(TAG, "[DISPLAY] API error: unable to determine practices.");
            break;
        default:
            ESP_LOGW(TAG, "[DISPLAY] Unknown state.");
            break;
    }
    // Optionally, update the actual display using esp_lcd functions.
}
