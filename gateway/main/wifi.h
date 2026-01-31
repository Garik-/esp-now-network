#pragma once

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "closer.h"
#include "config.h"

#define WAIT_STA_GOT_IP_MAX_MS 10000 // TODO: make configurable

esp_err_t wifi_start(closer_handle_t closer);
esp_err_t wifi_connect();