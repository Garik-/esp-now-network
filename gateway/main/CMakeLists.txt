idf_component_register(SRCS "httpd.c" "settings.c" "wifi.c" "espnow.c" "main.c"
                    PRIV_REQUIRES nvs_flash esp_event esp_netif esp_wifi mqtt esp_http_server
                    INCLUDE_DIRS "."
                     EMBED_FILES "../../gateway_settings/dist/index.html.gz")
