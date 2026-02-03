#pragma once

#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"

typedef esp_now_send_status_t node_send_status_t;

esp_err_t node_init(uint8_t channel, const uint8_t *mac);
esp_err_t node_broadcast(const uint8_t *data, size_t len, node_send_status_t *out_status, TickType_t xTicksToWait);