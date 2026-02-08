#include "esp_check.h"

#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "config.h"
#include "settings.h"
#include "wifi.h"

static const char *const TAG = "wifi_gateway";

static esp_netif_t *s_sta_netif = NULL;

static void handler_on_sta_got_ip(void *arg, __attribute__((unused)) esp_event_base_t event_base,
                                  __attribute__((unused)) int32_t event_id, void *event_data) {
    const TaskHandle_t to_notify = (const TaskHandle_t)arg;
    if (to_notify == NULL) {
        return;
    }

    const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
    if (event->esp_netif != s_sta_netif) {
        ESP_LOGW(TAG, "Got IP event for unknown netif");
        return;
    }

    ESP_LOGI(TAG, "Got IPv4 event, address: " IPSTR, IP2STR(&event->ip_info.ip));

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

static void wifi_event_handler(__attribute__((unused)) void *arg, esp_event_base_t event_base, int32_t event_id,
                               __attribute__((unused)) void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGI(TAG, "retry to connect to the AP");
        esp_wifi_connect();
    }
}

static esp_err_t wifi_wait_ip(TickType_t xTicksToWait) {
    if (xTaskNotifyWait(pdFALSE, ULONG_MAX, NULL, xTicksToWait) != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

static esp_err_t wifi_register_handlers() {
    ESP_RETURN_ON_ERROR(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL), TAG,
                        "esp_event_handler_register");

    ESP_RETURN_ON_ERROR(
        esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &handler_on_sta_got_ip, xTaskGetCurrentTaskHandle()),
        TAG, "esp_event_handler_register");

    return ESP_OK;
}

static esp_err_t wifi_set_sta_config(void) {
    wifi_config_t wifi_config = {
        .sta =
            {
                .ssid = "",
                .password = "",
            },
    };

    strlcpy((char *)wifi_config.sta.ssid, settings_wifi_ssid(), sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, settings_wifi_password(), sizeof(wifi_config.sta.password));

    return esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
}

esp_err_t wifi_start(closer_handle_t closer, __attribute__((unused)) void *arg) {
    DEFER(esp_netif_init(), closer, esp_netif_deinit);
    DEFER(esp_event_loop_create_default(), closer, esp_event_loop_delete_default);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    DEFER(esp_wifi_init(&cfg), closer, esp_wifi_deinit);

    s_sta_netif = esp_netif_create_default_wifi_sta();
    esp_netif_create_default_wifi_ap();

    ESP_RETURN_ON_ERROR(esp_wifi_set_storage(WIFI_STORAGE_RAM), TAG, "esp_wifi_set_storage");
    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(GATEWAY_WIFI_MODE), TAG, "esp_wifi_set_mode");

    ESP_RETURN_ON_ERROR(wifi_register_handlers(), TAG, "wifi_register_handlers");
    ESP_RETURN_ON_ERROR(wifi_set_sta_config(), TAG, "esp_wifi_set_config");

    DEFER(esp_wifi_start(), closer, esp_wifi_stop);

    ESP_RETURN_ON_ERROR(esp_wifi_set_channel(GATEWAY_WIFI_CHANEL, WIFI_SECOND_CHAN_NONE), TAG, "esp_wifi_set_channel");
    ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "esp_wifi_connect");

    return wifi_wait_ip(pdMS_TO_TICKS(WAIT_STA_GOT_IP_MAX_MS));
}
