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

#define QUEUE_SIZE 5
#define QUEUE_RECEIVE_TIMEOUT portMAX_DELAY // TODO: adjust timeout
#define QUEUE_SEND_TIMEOUT portMAX_DELAY    // TODO: adjust timeout

#define WAIT_NOTIFICATION pdMS_TO_TICKS(512)

#define ESPNOW_CHANNEL 6

static const char *TAG = "NODE";

#define START_FLAG 0x7E

typedef struct {
    uint8_t start_flag;
} __attribute__((packed)) packet_data_t;

static TaskHandle_t xTaskToNotify = NULL;
static QueueHandle_t s_event_queue = NULL;

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

static void send_task(__attribute__((unused)) void *pvParameter) {
    __atomic_store_n(&xTaskToNotify, xTaskGetCurrentTaskHandle(), __ATOMIC_SEQ_CST);

    BaseType_t xResult;
    uint32_t status;
    packet_data_t data;

    uint8_t broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

    for (;;) {
        if (xQueueReceive(s_event_queue, &data, QUEUE_RECEIVE_TIMEOUT) == pdTRUE) {
            if (esp_now_send(broadcast_mac, (const uint8_t *)&data, sizeof(packet_data_t)) != ESP_OK) {
                ESP_LOGW(TAG, "Send error");

                continue;
            }

            xResult = xTaskNotifyWait(pdFALSE,   /* Don't clear bits on entry. */
                                      ULONG_MAX, /* Clear all bits on exit. */
                                      &status,   /* Stores the notified value. */
                                      WAIT_NOTIFICATION);

            if (xResult != pdPASS) {
                ESP_LOGW(TAG, "No notification received within the timeout period");
            }
        }
    }
}

static esp_err_t espnow_init() {
    s_event_queue = xQueueCreate(QUEUE_SIZE, sizeof(packet_data_t));

    if (s_event_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return ESP_FAIL;
    }

    // configASSERT(s_event_queue);

    TRY(esp_now_init());
    TRY(esp_now_register_send_cb(send_cb));

    esp_now_peer_info_t peer = {
        .channel = ESPNOW_CHANNEL,
        .ifidx = ESP_IF_WIFI_STA,
        .encrypt = false,
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    };

    TRY(esp_now_add_peer(&peer));

    xTaskCreate(send_task, "send_task", 2048, NULL, tskIDLE_PRIORITY + 1, NULL);

    return ESP_OK;
}

__attribute__((cold)) static esp_err_t app_run() {
    TRY(nvs_init());
    TRY(wifi_init(ESPNOW_CHANNEL, NULL));
    TRY(espnow_init());

    packet_data_t data = {
        .start_flag = START_FLAG,
    };

    for (;;) {
        if (xQueueSend(s_event_queue, &data, QUEUE_SEND_TIMEOUT) != pdTRUE) {
            ESP_LOGE(TAG, "Failed to send to queue");
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
