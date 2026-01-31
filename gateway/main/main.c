#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "main_gateway";

#define CLOSER_IMPLEMENTATION
#include "closer.h"

#include "espnow.h"
#include "wifi.h"

__attribute__((cold)) static esp_err_t nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (unlikely(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        ret = nvs_flash_init();
    }

    return ret;
}

esp_err_t app_run() {
    ESP_RETURN_ON_ERROR(nvs_init(), TAG, "nvs_init");
    ESP_RETURN_ON_ERROR(with_closer(wifi_start), TAG, "wifi_start");
    ESP_RETURN_ON_ERROR(with_closer(espnow_start), TAG, "espnow_start");
    ESP_RETURN_ON_ERROR(wifi_connect(), TAG, "wifi_connect");

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
