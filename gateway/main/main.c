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

__attribute__((cold)) static esp_err_t nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (unlikely(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        ret = nvs_flash_init();
    }

    return ret;
}

#define WAIT_MQTT_CONNECTION_TIMEOUT_MS 10000

static void esp_mqtt_connected_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id,
                                             void *event_data) {
    if (unlikely(arg == NULL)) {
        return;
    }

    if (unlikely(event_id != MQTT_EVENT_CONNECTED)) {
        ESP_LOGW(TAG, "unexpected event_id: %d", event_id);
        return;
    }

    if (xPortInIsrContext()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR((TaskHandle_t)arg, 0, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else {
        xTaskNotify((TaskHandle_t)arg, 0, eSetValueWithOverwrite);
    }
}

__attribute__((cold)) static esp_err_t mqtt_app_start() {

    esp_err_t err;

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = GATEWAY_BROKER_URL,
        .credentials.username = GATEWAY_BROKER_USERNAME,
        .credentials.authentication.password = GATEWAY_BROKER_PASSWORD,
    };

    TaskHandle_t xTaskToNotify = xTaskGetCurrentTaskHandle();

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (unlikely(s_client == NULL)) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    err =
        esp_mqtt_client_register_event(s_client, MQTT_EVENT_CONNECTED, esp_mqtt_connected_event_handler, xTaskToNotify);
    if (unlikely(err != ESP_OK)) {
        ESP_LOGE(TAG, "Failed to register MQTT event handler: %s", esp_err_to_name(err));
        esp_mqtt_client_destroy(s_client);

        s_client = NULL;
        return err;
    }

    err = esp_mqtt_client_start(s_client);

    if (err == ESP_OK) {
        if (xTaskNotifyWait(pdFALSE, ULONG_MAX, NULL, pdMS_TO_TICKS(WAIT_MQTT_CONNECTION_TIMEOUT_MS)) == pdPASS) {
            return ESP_OK;
        }

        err = ESP_ERR_TIMEOUT;
    }

    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);

    s_client = NULL;

    return err;
}

espnow_rx_handler_t handle(const espnow_rx_t *rx) {
    if (s_client == NULL) {
        ESP_LOGW(TAG, "mqtt client is not initialized");
        return ESP_OK;
    }

    char topic[27];
    snprintf(topic, sizeof(topic), "/device/" MACSTR "", MAC2STR(rx->mac_addr));

    int msg_id = esp_mqtt_client_publish(s_client, topic, (const char *)rx->data, rx->len, 0, 1);
    ESP_LOGI(TAG, "publish message, topic=%s len=%d msg_id=%d", topic, rx->len, msg_id);

    return ESP_OK;
}

esp_err_t app_run() {
    ESP_RETURN_ON_ERROR(nvs_init(), TAG, "nvs_init");
    ESP_RETURN_ON_ERROR(with_closer(wifi_start, NULL), TAG, "wifi_start");
    ESP_RETURN_ON_ERROR(with_closer(espnow_start, &handle), TAG, "espnow_start");
    ESP_RETURN_ON_ERROR(wifi_connect(), TAG, "wifi_connect"); // TODO: add reconnect on disconnect

    ESP_RETURN_ON_ERROR(mqtt_app_start(), TAG, "mqtt_app_start");

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
