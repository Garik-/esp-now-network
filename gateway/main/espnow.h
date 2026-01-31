#pragma once

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "closer.h"
#include "config.h"

esp_err_t espnow_start(closer_handle_t closer);