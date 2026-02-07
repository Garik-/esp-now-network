#pragma once

#include "closer.h"

#include "esp_err.h"
#include "esp_now.h"

#define DATA_BUFFER_SIZE ESP_NOW_MAX_DATA_LEN

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t data[DATA_BUFFER_SIZE];
    size_t len;
} espnow_rx_t;

typedef esp_err_t (*espnow_rx_handler_t)(const espnow_rx_t *rx);
esp_err_t espnow_start(closer_handle_t close, void *arg);