#include "wifi.h"

static const char *TAG = "wifi_gateway";

static esp_netif_t *s_sta_netif;

static esp_err_t delete_default_wifi_driver_and_handlers() {
    if (unlikely(s_sta_netif == NULL)) {
        return ESP_OK;
    }

    return esp_wifi_clear_default_wifi_driver_and_handlers(s_sta_netif);
}

static esp_err_t sta_netif_destroy() {
    if (unlikely(s_sta_netif == NULL)) {
        return ESP_OK;
    }

    esp_netif_destroy(s_sta_netif);
    s_sta_netif = NULL;
    return ESP_OK;
}

esp_err_t wifi_start(closer_handle_t closer) {
    DEFER(esp_netif_init(), closer, esp_netif_deinit);
    DEFER(esp_event_loop_create_default(), closer, esp_event_loop_delete_default);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    DEFER(esp_wifi_init(&cfg), closer, esp_wifi_deinit);

    esp_netif_inherent_config_t esp_netif_config = ESP_NETIF_INHERENT_DEFAULT_WIFI_STA();
    s_sta_netif = esp_netif_create_wifi(WIFI_IF_STA, &esp_netif_config);

    if (unlikely(s_sta_netif == NULL)) {
        ESP_LOGE(TAG, "esp_netif_create_wifi");
        return ESP_FAIL;
    }
    closer_add(closer, sta_netif_destroy, "sta_netif_destroy");

    DEFER(esp_wifi_set_default_wifi_sta_handlers(), closer, delete_default_wifi_driver_and_handlers);

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "esp_wifi_set_storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(GATEWAY_WIFI_MODE), TAG, "esp_wifi_set_mode");

    DEFER(esp_wifi_start(), closer, esp_wifi_stop);

    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(GATEWAY_WIFI_CHANEL, WIFI_SECOND_CHAN_NONE), TAG, "esp_wifi_set_channel");

    int8_t pwr;
    ESP_RETURN_ON_ERROR(esp_wifi_get_max_tx_power(&pwr), TAG, "esp_wifi_get_max_tx_power");
    ESP_LOGI(TAG, "WiFi TX power = %.2f dBm, pwr=%d", pwr * 0.25, pwr);

    return ESP_OK;
}