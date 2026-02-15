set(srcs
    "httpd.c"
    "settings.c"
    "wifi.c"
    "espnow.c"
    "main.c"
)

set(priv_requires
    nvs_flash
    esp_event
    esp_netif
    esp_wifi
    mqtt
    esp_http_server
)

if(CONFIG_GATEWAY_ENABLE_SSE_LOGS)
    list(APPEND srcs "logs.c")
    list(APPEND priv_requires esp_ringbuf)
endif()

idf_component_register(
    SRCS ${srcs}
    PRIV_REQUIRES ${priv_requires}
    INCLUDE_DIRS "."
    EMBED_FILES "../../gateway_settings/dist/index.html.gz"
)
