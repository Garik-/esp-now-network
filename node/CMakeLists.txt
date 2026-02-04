idf_component_register(SRCS "node.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif
                    REQUIRES esp_wifi
                    INCLUDE_DIRS "include")
