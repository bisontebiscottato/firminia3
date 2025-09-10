set(srcs 
    "main_flow.c"
    "api_manager.c"
    "ble_manager.c"
    "display_manager.c"
    "wifi_manager.c"
    "device_config.c"
    "translations.c"
    "qr_image.c"
    "ota_manager.c"
    )

    idf_component_register(SRCS ${srcs}
                    PRIV_REQUIRES esp_event nvs_flash esp_netif mbedtls json esp_driver_gpio esp_wifi esp_timer bt esp_lcd esp_https_ota app_update
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash esp_http_client app_update)