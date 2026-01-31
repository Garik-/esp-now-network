#pragma once

#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include "closer.h"
#include "config.h"

esp_err_t wifi_start(closer_handle_t closer);