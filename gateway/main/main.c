#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

static const char *TAG = "main_gateway";

#define CLOSER_IMPLEMENTATION
#include "closer.h"

#include "config.h"
#include "espnow.h"
#include "wifi.h"

static esp_mqtt_client_handle_t s_client = NULL;

#define TRY(expr) ESP_RETURN_ON_ERROR((expr), TAG, "%s:%d", __func__, __LINE__)

__attribute__((cold)) static esp_err_t nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (unlikely(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        ret = nvs_flash_init();
    }

    return ret;
}

__attribute__((cold)) static esp_err_t mqtt_app_start() {
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = GATEWAY_BROKER_URL,
        .credentials.username = GATEWAY_BROKER_USERNAME,
        .credentials.authentication.password = GATEWAY_BROKER_PASSWORD,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (unlikely(s_client == NULL)) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    return esp_mqtt_client_start(s_client);
}

espnow_rx_handler_t handle(const espnow_rx_t *rx) {
    if (s_client == NULL) {
        ESP_LOGW(TAG, "mqtt client is not initialized");
        return ESP_OK;
    }

    char topic[27];
    snprintf(topic, sizeof(topic), "/device/" MACSTR "", MAC2STR(rx->mac_addr));

    int msg_id = esp_mqtt_client_publish(s_client, topic, (const char *)rx->data, rx->len, GATEWAY_BROKER_QOS,
                                         GATEWAY_BROKER_RETAIN);
    ESP_LOGI(TAG, "publish message, topic=%s len=%d msg_id=%d", topic, rx->len, msg_id);

    return ESP_OK;
}

esp_err_t app_run() {
    TRY(nvs_init());
    TRY(with_closer(wifi_start, NULL));
    TRY(with_closer(espnow_start, &handle));
    TRY(mqtt_app_start());

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
