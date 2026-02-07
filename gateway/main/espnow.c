#include "esp_check.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_now.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#include "mqtt_client.h"

#include "config.h"

#include "espnow.h"

#define QUEUE_SIZE 6
#define MAXDELAY_MS 512

#define DATA_BUFFER_SIZE ESP_NOW_MAX_DATA_LEN

static const char *TAG = "esp_now_gateway";

#define TRY(expr) ESP_RETURN_ON_ERROR((expr), TAG, "%s:%d", __func__, __LINE__)

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} event_recv_cb_t;

static QueueHandle_t s_event_queue = NULL;

static uint8_t data_buffer_pool[QUEUE_SIZE][DATA_BUFFER_SIZE];
static uint8_t data_buffer_index = 0;

static uint8_t *get_static_buffer(int len) {
    if (unlikely(len > DATA_BUFFER_SIZE)) {
        return NULL;
    }

    uint8_t current_index = __atomic_fetch_add(&data_buffer_index, 1, __ATOMIC_SEQ_CST);
    current_index %= QUEUE_SIZE;

    return data_buffer_pool[current_index];
}

static esp_err_t espnow_deinit() {
    vQueueDelete(s_event_queue);
    s_event_queue = NULL;
    return esp_now_deinit();
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len) {
    event_recv_cb_t recv_cb = {0};
    uint8_t *mac_addr = recv_info->src_addr;
    // uint8_t *des_addr = recv_info->des_addr;

    if (mac_addr == NULL || data == NULL || len <= 0 || len > DATA_BUFFER_SIZE) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    // TODO: add check dest_addr if needed
    memcpy(recv_cb.mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb.data = get_static_buffer(len);
    if (recv_cb.data == NULL) {
        ESP_LOGE(TAG, "get_static_buffer receive data fail");
        return;
    }
    memcpy(recv_cb.data, data, len);
    recv_cb.data_len = len;
    if (xQueueSend(s_event_queue, &recv_cb, pdMS_TO_TICKS(MAXDELAY_MS)) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
    }
}

static void espnow_task(void *pvParameter) {
    event_recv_cb_t recv_cb;

    ESP_LOGI(TAG, "start receive peer data task");

    char mac_str[27];

    for (;;) {
        if (s_event_queue == NULL) {
            ESP_LOGE(TAG, "event queue is NULL");
            vTaskDelete(NULL);
        }

        if (xQueueReceive(s_event_queue, &recv_cb, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "xQueueReceive failed");
            continue;
        }

        snprintf(mac_str, sizeof(mac_str), "/device/" MACSTR "", MAC2STR(recv_cb.mac_addr));

        ESP_LOGI(TAG, "receive data from %s, len: %d", mac_str, recv_cb.data_len); // TODO: delete
    }
}

static esp_err_t espnow_init() {
    s_event_queue = xQueueCreate(QUEUE_SIZE, sizeof(event_recv_cb_t));
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

    xTaskCreate(espnow_task, "espnow_task", 1024 * 2, NULL, 4, NULL); // TODO: adjust stack size

    return ESP_OK;
}

esp_err_t espnow_start(closer_handle_t closer) {
    DEFER(espnow_init(), closer, espnow_deinit);

    return ESP_OK;
}