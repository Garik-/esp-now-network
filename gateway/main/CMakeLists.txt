idf_component_register(SRCS "wifi.c" "espnow.c" "main.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi mqtt
                    INCLUDE_DIRS "."
                    INCLUDE_DIRS "../../common")
