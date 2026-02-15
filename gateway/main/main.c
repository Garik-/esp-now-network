#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_timer.h"
#include "mdns.h"
#include "mqtt_client.h"
#include "nvs_flash.h"

static const char *const TAG = "main_gateway";

#define CLOSER_IMPLEMENTATION
#include "closer.h"

#include "config.h"
#include "espnow.h"
#include "httpd.h"
#if CONFIG_GATEWAY_ENABLE_SSE_LOGS
#include "logs.h"
#endif
#include "settings.h"
#include "wifi.h"

static esp_mqtt_client_handle_t s_client = NULL;

#define MQTT_TOPIC_MAX_LEN 27 // "/device/" + MACSTR + '\0'

__attribute__((cold)) static esp_err_t nvs_init(void) {
    esp_err_t ret = nvs_flash_init();
    if (unlikely(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        ret = nvs_flash_init();
    }

    return ret;
}

__attribute__((cold)) static esp_err_t mqtt_app_start(void) {
    esp_err_t err = ESP_OK;
    const esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = settings_mqtt_uri(),
        .credentials.username = settings_mqtt_user(),
        .credentials.authentication.password = settings_mqtt_password(),
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to initialize MQTT client");
        return ESP_FAIL;
    }

    err = esp_mqtt_client_start(s_client);
    if (err != ESP_OK) {
        esp_mqtt_client_destroy(s_client);
        s_client = NULL;
    }

    return err;
}

static esp_err_t mdns_start(void) {
    ESP_RETURN_ON_ERROR(mdns_init(), TAG, "mdns_init");

    ESP_RETURN_ON_ERROR(mdns_hostname_set(GATEWAY_MDNS_NAME), TAG, "mdns_hostname_set failed");
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", GATEWAY_MDNS_NAME);

    ESP_RETURN_ON_ERROR(mdns_instance_name_set(GATEWAY_MDNS_INSTANCE_NAME), TAG, "mdns_instance_name_set failed");
    ESP_RETURN_ON_ERROR(mdns_service_add(NULL, "_http", "_tcp", GATEWAY_HTTP_PORT, NULL, 0), TAG,
                        "mdns_service_add failed");

    return ESP_OK;
}

static esp_err_t handle(const espnow_rx_t *rx) {
    if (s_client == NULL) {
        ESP_LOGW(TAG, "mqtt client is not initialized");
        return ESP_OK;
    }

    char topic[MQTT_TOPIC_MAX_LEN];
    int topic_n = snprintf(topic, sizeof(topic), "/device/" MACSTR "", MAC2STR(rx->mac_addr));
    if (topic_n < 0 || (size_t)topic_n >= sizeof(topic)) {
        ESP_LOGE(TAG, "Failed to format MQTT topic");
        return ESP_FAIL;
    }

    int msg_id = esp_mqtt_client_publish(s_client, topic, (const char *)rx->data, rx->len, GATEWAY_BROKER_QOS,
                                         GATEWAY_BROKER_RETAIN);

    ESP_LOGI(TAG, "mqtt publish, topic=%s len=%u msg_id=%d", topic, (unsigned)rx->len, msg_id);

    if (msg_id < 0) {
        return ESP_FAIL;
    }

#if CONFIG_GATEWAY_ENABLE_SSE_LOGS
    char line[256];
    int n = snprintf(line, sizeof(line), "data:%s,%u,%d\n\n", topic, (unsigned)rx->len, msg_id);
    if (n < 0 || (size_t)n >= sizeof(line)) {
        ESP_LOGE(TAG, "Failed to format SSE log line");
        return ESP_FAIL;
    }

    return logs_push(line, (size_t)n);
#endif

    return ESP_OK;
}

static esp_err_t app_run(void) {
    ESP_RETURN_ON_ERROR(nvs_init(), TAG, "nvs_init");
    ESP_RETURN_ON_ERROR(settings_init(), TAG, "settings_init");
    ESP_RETURN_ON_ERROR(with_closer(wifi_start, NULL), TAG, "wifi_start");
    ESP_RETURN_ON_ERROR(with_closer(espnow_start, &handle), TAG, "espnow_start");
    ESP_RETURN_ON_ERROR(mdns_start(), TAG, "mdns_start");
    ESP_RETURN_ON_ERROR(mqtt_app_start(), TAG, "mqtt_app_start");
    ESP_RETURN_ON_ERROR(httpd_start_server(), TAG, "httpd_start_server");

    return ESP_OK;
}

void app_main(void) {
    esp_err_t err = app_run();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "app run failed: %s", esp_err_to_name(err));
    }
}
