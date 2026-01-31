#include "wifi.h"

static const char *TAG = "wifi_gateway";

static esp_netif_t *s_sta_netif = NULL;
static TaskHandle_t xTaskToNotify = NULL;

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

static void handler_on_sta_got_ip(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    if (event->esp_netif != s_sta_netif) {
        ESP_LOGW(TAG, "Got IP event for unknown netif");
        return;
    }

    ESP_LOGI(TAG, "Got IPv4 event, address: " IPSTR, IP2STR(&event->ip_info.ip));

    TaskHandle_t to_notify = __atomic_load_n(&xTaskToNotify, __ATOMIC_SEQ_CST);
    if (to_notify) {
        if (xPortInIsrContext()) {
            BaseType_t xHigherPriorityTaskWoken = pdFALSE;
            xTaskNotifyFromISR(to_notify, 0, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
            if (xHigherPriorityTaskWoken) {
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
        } else {
            xTaskNotify(to_notify, 0, eSetValueWithOverwrite);
        }
    }
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA started");
            break;
        case WIFI_EVENT_STA_CONNECTED: {
            wifi_event_sta_connected_t *evt = (wifi_event_sta_connected_t *)event_data;
            ESP_LOGI(TAG, "Connected to AP, channel %d", evt->channel);
            break;
        }
        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGI(TAG, "Disconnected from AP");
            break;
        default:
            break;
        }
    }
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

    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG,
                        "esp_event_handler_register");

    DEFER(esp_wifi_start(), closer, esp_wifi_stop);

    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(GATEWAY_WIFI_CHANEL, WIFI_SECOND_CHAN_NONE), TAG, "esp_wifi_set_channel");

    int8_t pwr;
    ESP_RETURN_ON_ERROR(esp_wifi_get_max_tx_power(&pwr), TAG, "esp_wifi_get_max_tx_power");
    ESP_LOGI(TAG, "WiFi TX power = %.2f dBm, pwr=%d", pwr * 0.25, pwr);

    return ESP_OK;
}

esp_err_t wifi_connect() {
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = GATEWAY_WIFI_SSID,
                .password = GATEWAY_WIFI_PASSWORD,
            },
    };

    ESP_LOGI(TAG, "Connecting to %s...", wifi_config.sta.ssid);
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "esp_wifi_set_config");

    __atomic_store_n(&xTaskToNotify, xTaskGetCurrentTaskHandle(), __ATOMIC_SEQ_CST);

    ESP_RETURN_ON_ERROR(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip, NULL), TAG,
                        "esp_event_handler_register");

    esp_err_t err = esp_wifi_connect();

    if (err != ESP_OK) {
        goto cleanup;
    }

    ESP_LOGI(TAG, "Waiting for IP address...");

    if (xTaskNotifyWait(pdFALSE, ULONG_MAX, NULL, pdMS_TO_TICKS(WAIT_STA_GOT_IP_MAX_MS)) != pdPASS) {
        err = ESP_ERR_TIMEOUT;
        ESP_LOGW(TAG, "No ip received within the timeout period");

        goto cleanup;
    }

    err = ESP_OK;

cleanup:

    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip); // TODO: спорно
    __atomic_store_n(&xTaskToNotify, NULL, __ATOMIC_SEQ_CST);

    return err;
}