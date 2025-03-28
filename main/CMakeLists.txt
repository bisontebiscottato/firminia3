set(srcs 
    "main.c"
    "device_config.c"
    )

    idf_component_register(SRCS ${srcs}
                    PRIV_REQUIRES esp_event nvs_flash esp_netif mbedtls json esp_driver_gpio esp_wifi esp_timer bt esp_lcd
                    INCLUDE_DIRS "."
                    REQUIRES bt nvs_flash)