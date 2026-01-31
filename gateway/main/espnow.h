#pragma once

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

esp_err_t espnow_init(uint8_t channel, wifi_interface_t ifidx);
esp_err_t espnow_deinit();