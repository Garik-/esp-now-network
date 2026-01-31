#include "espnow.h"

#define QUEUE_SIZE 6
#define MAXDELAY_MS 512

#define DATA_BUFFER_SIZE ESP_NOW_MAX_DATA_LEN

static const char *TAG = "esp_now_receiver";

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} event_recv_cb_t;

static QueueHandle_t s_event_queue = NULL;
static uint8_t s_peer_mac[ESP_NOW_ETH_ALEN] = {0x02, 0x12, 0x34, 0x56, 0x78, 0x9A}; // TODO: set from broadcast

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

esp_err_t espnow_deinit() {
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

    for (;;) {
        if (s_event_queue == NULL) {
            ESP_LOGE(TAG, "event queue is NULL");
            vTaskDelete(NULL);
        }

        if (xQueueReceive(s_event_queue, &recv_cb, portMAX_DELAY) != pdTRUE) {
            ESP_LOGE(TAG, "xQueueReceive failed");
            continue;
        }

        ESP_LOGI(TAG, "receive data from " MACSTR ", len: %d", MAC2STR(recv_cb.mac_addr),
                 recv_cb.data_len); // TODO: set LOGD
        // if (recv_cb.data) {
        //     data_parse(recv_cb.data, recv_cb.data_len);
        // }
    }
}

esp_err_t espnow_init(uint8_t channel, wifi_interface_t ifidx) {
    s_event_queue = xQueueCreate(QUEUE_SIZE, sizeof(event_recv_cb_t));
    if (unlikely(s_event_queue == NULL)) {
        ESP_LOGE(TAG, "create queue fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_RETURN_ON_ERROR(esp_now_init(), TAG, "esp_now_init");
    ESP_RETURN_ON_ERROR(esp_now_register_recv_cb(espnow_recv_cb), TAG, "esp_now_register_recv_cb");

    esp_now_peer_info_t peer = {
        .channel = channel,
        .ifidx = ifidx,
        .encrypt = false,
    };
    memcpy(peer.peer_addr, s_peer_mac, ESP_NOW_ETH_ALEN);

    ESP_RETURN_ON_ERROR(esp_now_add_peer(&peer), TAG, "esp_now_add_peer");

    xTaskCreate(espnow_task, "espnow_task", 2048, NULL, 4, NULL);

    return ESP_OK;
}