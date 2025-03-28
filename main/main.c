/* FirminIA v3 for AskMeSign
 * Andrea Mancini, biso@biso.it
 */

// Include di sistema
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

//Include librerie
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"

//Include locali
#include "device_config.h"


/* TAG progetto */
static const char *TAG = "Main";

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Load configuration from NVS, including dynamic parameters
    load_config_from_nvs();
    
    // Here you can use the configuration values (e.g., configure Wi-Fi, display settings, etc.)
}