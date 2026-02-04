#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "node.h"

static const char *TAG = "NODE";
static const uint8_t BROADCAST_MAC[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

static TaskHandle_t s_task_to_notify = NULL;

#define TRY(expr) ESP_RETURN_ON_ERROR((expr), TAG, "%s:%d", __func__, __LINE__)

__attribute__((cold)) static esp_err_t wifi_init(uint8_t channel, const uint8_t *mac) {
    TRY(esp_netif_init());
    TRY(esp_event_loop_create_default());
    const wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    TRY(esp_wifi_init(&cfg));
    TRY(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    TRY(esp_wifi_set_mode(WIFI_MODE_STA));
    TRY(esp_wifi_start());
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

static void send_cb(__attribute__((unused)) const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    TaskHandle_t to_notify = __atomic_load_n(&s_task_to_notify, __ATOMIC_SEQ_CST);
    if (unlikely(to_notify == NULL)) {
        return;
    }

    if (xPortInIsrContext()) {
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;
        xTaskNotifyFromISR(to_notify, status, eSetValueWithOverwrite, &xHigherPriorityTaskWoken);
        if (xHigherPriorityTaskWoken) {
            portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
        }
    } else {
        xTaskNotify(to_notify, status, eSetValueWithOverwrite);
    }
}

esp_err_t node_send(const uint8_t *peer_addr, const uint8_t *data, size_t len, esp_now_send_status_t *out_status,
                    TickType_t xTicksToWait) {
    if (unlikely(peer_addr == NULL || data == NULL || len == 0)) {
        return ESP_ERR_INVALID_ARG;
    }

    __atomic_store_n(&s_task_to_notify, xTaskGetCurrentTaskHandle(), __ATOMIC_SEQ_CST);

    const esp_err_t err = esp_now_send(peer_addr, data, len);
    if (unlikely(err != ESP_OK)) {
        return err;
    }

    const BaseType_t xResult = xTaskNotifyWait(pdFALSE, ULONG_MAX, (uint32_t *)out_status, xTicksToWait);

    if (unlikely(xResult != pdPASS)) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t node_broadcast(const uint8_t *data, size_t len, node_send_status_t *out_status, TickType_t xTicksToWait) {
    return node_send(BROADCAST_MAC, data, len, out_status, xTicksToWait);
}

__attribute__((cold)) static esp_err_t espnow_init(uint8_t channel) {
    TRY(esp_now_init());
    TRY(esp_now_register_send_cb(send_cb));

    const esp_now_peer_info_t peer = {.channel = channel,
                                      .ifidx = ESP_IF_WIFI_STA,
                                      .encrypt = false,
                                      .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};

    TRY(esp_now_add_peer(&peer));

    return ESP_OK;
}

__attribute__((cold)) esp_err_t node_init(uint8_t channel, const uint8_t *mac) {
    TRY(nvs_init());
    TRY(wifi_init(channel, mac));
    TRY(espnow_init(channel));

    return ESP_OK;
}