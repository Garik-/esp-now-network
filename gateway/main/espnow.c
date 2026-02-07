#include "esp_check.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "mqtt_client.h"

#include "config.h"
#include "espnow.h"

#define QUEUE_SIZE 6
#define MAXDELAY_MS 512

static const char *TAG = "esp_now_gateway";

#define TRY(expr) ESP_RETURN_ON_ERROR((expr), TAG, "%s:%d", __func__, __LINE__)

static QueueHandle_t s_event_queue = NULL;

static esp_err_t espnow_deinit() {
    vQueueDelete(s_event_queue);
    s_event_queue = NULL;
    return esp_now_deinit();
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (data == NULL || len <= 0 || len > DATA_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    espnow_rx_t rx;

    // TODO: add check dest_addr if needed
    memcpy(rx.mac_addr, recv_info->src_addr, ESP_NOW_ETH_ALEN);
    memcpy(rx.data, data, len);
    rx.len = len;

    if (xQueueSend(s_event_queue, &rx, pdMS_TO_TICKS(MAXDELAY_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
    }
}

static void espnow_task(void *handle_fn) {
    espnow_rx_t rx;

    ESP_LOGI(TAG, "start receive peer data task");

    for (;;) {
        if (s_event_queue == NULL) {
            ESP_LOGE(TAG, "event queue is NULL");
            vTaskDelete(NULL);
        }

        if (xQueueReceive(s_event_queue, &rx, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "xQueueReceive failed");
            continue;
        }

        ((espnow_rx_handler_t)handle_fn)(&rx);
    }
}

static esp_err_t espnow_init(espnow_rx_handler_t handle_fn) {
    s_event_queue = xQueueCreate(QUEUE_SIZE, sizeof(espnow_rx_t));
    if (unlikely(s_event_queue == NULL)) {
        ESP_LOGE(TAG, "create queue fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    TRY(esp_now_init());
    TRY(esp_now_register_recv_cb(espnow_recv_cb));

    const esp_now_peer_info_t peer = {
        .channel = GATEWAY_WIFI_CHANEL,
        .ifidx = GATEWAY_WIFI_IF,
        .encrypt = false,
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    };

    TRY(esp_now_add_peer(&peer));

    xTaskCreate(espnow_task, "espnow_task", 1024 * 4, handle_fn, 4, NULL); // TODO: adjust stack size

    return ESP_OK;
}

esp_err_t espnow_start(closer_handle_t closer, void *arg) {
    DEFER(espnow_init((espnow_rx_handler_t)arg), closer, espnow_deinit);

    return ESP_OK;
}