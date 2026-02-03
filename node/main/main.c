#include "esp_check.h"
#include "esp_err.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"

#define WAIT_NOTIFICATION pdMS_TO_TICKS(512)

#define ESPNOW_CHANNEL 6

static const char *TAG = "NODE";

#define START_FLAG 0x7E

typedef struct {
    uint8_t start_flag;
} __attribute__((packed)) packet_data_t;

static TaskHandle_t xTaskToNotify = NULL;

#define TRY(expr) ESP_RETURN_ON_ERROR((expr), TAG, "%s:%d", __func__, __LINE__)

__attribute__((cold)) static esp_err_t wifi_init(uint8_t channel, const uint8_t *mac) {
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

static void send_cb(const esp_now_send_info_t *tx_info, esp_now_send_status_t status) {
    TaskHandle_t to_notify = __atomic_load_n(&xTaskToNotify, __ATOMIC_SEQ_CST);
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

static esp_err_t send_broadcast(const uint8_t *data, size_t len, esp_now_send_status_t *out_status,
                                TickType_t xTicksToWait) {
    static uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    __atomic_store_n(&xTaskToNotify, xTaskGetCurrentTaskHandle(),
                     __ATOMIC_SEQ_CST); // TODO: фигово наверное если часто вызывается или вызывается из разных потоков

    esp_err_t err = esp_now_send(broadcast_mac, data, len);
    if (err != ESP_OK) {
        return err;
    }

    BaseType_t xResult = xTaskNotifyWait(pdFALSE, ULONG_MAX, (uint32_t *)out_status, xTicksToWait);

    if (xResult != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

__attribute__((cold)) static esp_err_t espnow_init() {
    TRY(esp_now_init());
    TRY(esp_now_register_send_cb(send_cb));

    esp_now_peer_info_t peer = {
        .channel = ESPNOW_CHANNEL,
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    };

    TRY(esp_now_add_peer(&peer));

    return ESP_OK;
}

__attribute__((cold)) static esp_err_t app_run() {
    TRY(nvs_init());
    TRY(wifi_init(ESPNOW_CHANNEL, NULL));
    TRY(espnow_init());

    packet_data_t data = {
        .start_flag = START_FLAG,
    };

    esp_now_send_status_t status;
    esp_err_t err;

    for (;;) {
        err = send_broadcast((const uint8_t *)&data, sizeof(packet_data_t), &status, WAIT_NOTIFICATION);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send broadcast: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Broadcast sent, status: %s",
                     status == ESP_NOW_SEND_SUCCESS ? "ESP_NOW_SEND_SUCCESS" : "ESP_NOW_SEND_FAIL");
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
