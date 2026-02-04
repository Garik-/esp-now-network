cmake_minimum_required(VERSION 3.16)

file(GLOB SOURCES src/node.c)
# list(APPEND SOURCES )

list(APPEND pub_requires esp_wifi)
list(APPEND priv_requires nvs_flash esp_event esp_netif)

idf_component_register(
    SRCS ${SOURCES}
    INCLUDE_DIRS include
    PRIV_INCLUDE_DIRS src
    REQUIRES ${pub_requires}
    PRIV_REQUIRES ${priv_requires}
)
