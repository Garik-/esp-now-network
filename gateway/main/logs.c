#include "logs.h"
#include "esp_log.h"

#define BUF_SIZE 1024

static RingbufHandle_t log_rb = NULL;
static const char *TAG = "logs";

esp_err_t logs_init(RingbufHandle_t *out) {
    if (unlikely(log_rb != NULL)) {
        ESP_LOGW(TAG, "Log system already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    log_rb = xRingbufferCreate(BUF_SIZE, RINGBUF_TYPE_BYTEBUF);
    if (unlikely(log_rb == NULL)) {
        ESP_LOGE(TAG, "Failed to create ring buffer (%u bytes)", BUF_SIZE);
        return ESP_ERR_NO_MEM;
    }

    *out = log_rb;
    return ESP_OK;
}

esp_err_t logs_deinit(void) {
    if (unlikely(log_rb == NULL)) {
        ESP_LOGW(TAG, "Log system is not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    vRingbufferDelete(log_rb);
    log_rb = NULL;
    return ESP_OK;
}

esp_err_t logs_push(const char *msg, size_t len) {
    if (log_rb == NULL || len == 0) {
        return ESP_OK;
    }

    if (unlikely(msg == NULL)) {
        ESP_LOGE(TAG, "logs_push called with NULL message");
        return ESP_ERR_INVALID_ARG;
    }

    xRingbufferSend(log_rb, msg, len, 0);

    return ESP_OK;
}
