#include "esp_check.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "mqtt_client.h"

#include "config.h"
#include "espnow.h"

#define QUEUE_SIZE 6     // Max pending ESP-NOW packets before drop.
#define MAXDELAY_MS 512  // Max queue wait/send time to avoid blocking callbacks.
#define STACK_DEPTH 4096 // Stack size for ESP-NOW task.

static const char *const TAG = "esp_now_gateway";

static inline esp_err_t check_err(esp_err_t err, const char *what) {
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "%s failed: %s", what, esp_err_to_name(err));
    }
    return err;
}

static QueueHandle_t s_event_queue = NULL;

static esp_err_t espnow_deinit() {
    espnow_rx_t rx = {.data = NULL, .len = 0}; // sentinel for task exit
    xQueueSend(s_event_queue, &rx, pdMS_TO_TICKS(MAXDELAY_MS));

    vQueueDelete(s_event_queue);
    s_event_queue = NULL;
    return esp_now_deinit();
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    if (recv_info == NULL || data == NULL || len <= 0 || len > DATA_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    if (s_event_queue == NULL) {
        ESP_LOGW(TAG, "Receive queue not initialized");
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
            break;
        }

        if (xQueueReceive(s_event_queue, &rx, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        if (rx.data == NULL && rx.len == 0) { // sentinel for task exit
            break;
        }

        ((espnow_rx_handler_t)handle_fn)(&rx);
    }

    vTaskDelete(NULL);
}

static esp_err_t espnow_init(espnow_rx_handler_t handle_fn) {
    s_event_queue = xQueueCreate(QUEUE_SIZE, sizeof(espnow_rx_t));
    if (unlikely(s_event_queue == NULL)) {
        ESP_LOGE(TAG, "create queue fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(espnow_recv_cb), TAG, "esp_now_register_recv_cb");

    const esp_now_peer_info_t peer = {
        .channel = GATEWAY_WIFI_CHANEL,
        .ifidx = GATEWAY_WIFI_IF,
        .encrypt = false,
        .peer_addr = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    };

    ESP_RETURN_ON_ERROR(esp_now_add_peer(&peer), TAG, "esp_now_add_peer");

    if (xTaskCreate(espnow_task, "espnow_task", STACK_DEPTH, handle_fn, tskIDLE_PRIORITY + 1, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create espnow_task");
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t espnow_start(closer_handle_t closer, void *arg) {
    DEFER(espnow_init((espnow_rx_handler_t)arg), closer, espnow_deinit);

    return ESP_OK;
}
