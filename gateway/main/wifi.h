#pragma once

#include "closer.h"
#include "esp_err.h"

#define WAIT_STA_GOT_IP_MAX_MS 10000 // TODO: make configurable

esp_err_t wifi_start(closer_handle_t closer);
esp_err_t wifi_connect();
uint8_t wifi_get_channel();