#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

static const char *TAG = "NODE";

#define TRY(expr) ESP_RETURN_ON_ERROR((expr), TAG, "%s:%d", __func__, __LINE__)

__attribute__((cold)) static esp_err_t wifi_init(uint8_t channel, const uint8_t *mac) {
    ESP_LOGI(TAG, "wifi_init: channel=%d mac=" MACSTR, channel, MAC2STR(mac));

    TRY(esp_netif_init());
    TRY(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    TRY(esp_wifi_init(&cfg));
    TRY(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    TRY(esp_wifi_set_mode(WIFI_MODE_STA));

    TRY(esp_wifi_start());

    int8_t pwr;
    TRY(esp_wifi_get_max_tx_power(&pwr));
    ESP_LOGI(TAG, "WiFi TX power = %.2f dBm, pwr=%d", pwr * 0.25, pwr);

    TRY(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

    if (mac != NULL) {
        TRY(esp_wifi_set_mac(WIFI_IF_STA, mac));
    }

    return ESP_OK;
}

__attribute__((cold)) static esp_err_t nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (unlikely(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        TRY(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    return ret;
}

static esp_err_t espnow_init() {
    TRY(esp_now_init());
    // TRY(esp_now_register_send_cb(espnow_send_cb), TAG, "esp_now_register_send_cb fail");

    return ESP_OK;
}

__attribute__((cold)) static esp_err_t app_run() {
    TRY(nvs_init());
    TRY(wifi_init(6, NULL)); // TODO: set params from config
    TRY(espnow_init());

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
