#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"

#include "nvs_flash.h"

#define GATEWAY_WIFI_MODE WIFI_MODE_APSTA
#define GATEWAY_WIFI_IF ESP_IF_WIFI_AP
#define GATEWAY_WIFI_CHANEL CONFIG_ESPNOW_CHANNEL

#include "espnow.h"

static const char *TAG = "main_gateway";

static esp_netif_t *s_sta_netif;

#define CLOSER_IMPLEMENTATION
#include "closer.h"

typedef esp_err_t (*define_fn_t)(closer_handle_t);
static esp_err_t with_closer(define_fn_t fn) {
    esp_err_t err;
    closer_handle_t closer = NULL;

    err = closer_create(&closer);
    if (unlikely(err != ESP_OK))
        return err;

    err = fn(closer);
    if (err != ESP_OK) {
        closer_close(closer);
    }

    closer_destroy(closer);

    return err;
}

#define DEFER(call, closer, cleanup_fn)                                                                                \
    do {                                                                                                               \
        esp_err_t err_rc_ = (call);                                                                                    \
        if (err_rc_ != ESP_OK) {                                                                                       \
            ESP_LOGE(TAG, "%s failed: %s", #call, esp_err_to_name(err_rc_));                                           \
            return err_rc_;                                                                                            \
        }                                                                                                              \
        closer_add((closer), (cleanup_fn), #cleanup_fn);                                                               \
    } while (0)

__attribute__((cold)) static esp_err_t nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (unlikely(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        ret = nvs_flash_init();
    }

    return ret;
}

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

static esp_err_t wifi_start(closer_handle_t closer) {
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

static esp_err_t espnow_start(closer_handle_t closer) {
    DEFER(espnow_init(GATEWAY_WIFI_CHANEL, GATEWAY_WIFI_IF), closer, espnow_deinit);

    return ESP_OK;
}

esp_err_t app_run() {
    ESP_RETURN_ON_ERROR(nvs_init(), TAG, "nvs_init");
    ESP_RETURN_ON_ERROR(with_closer(wifi_start), TAG, "wifi_start");
    ESP_RETURN_ON_ERROR(with_closer(espnow_start), TAG, "espnow_start");

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
