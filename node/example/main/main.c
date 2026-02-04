#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"

#include "unity.h"

#include "node.h"

#include "freertos/FreeRTOS.h"

#define WAIT_NOTIFICATION pdMS_TO_TICKS(512)
#define ESPNOW_CHANNEL 6

static const char *TAG = "MAIN";
#define TRY(expr) ESP_RETURN_ON_ERROR((expr), TAG, "%s:%d", __func__, __LINE__)

#define START_FLAG 0x7E

typedef struct {
    uint8_t start_flag;
} __attribute__((packed)) packet_data_t;

__attribute__((cold)) static esp_err_t app_run() {
    TRY(node_init(ESPNOW_CHANNEL, NULL));

    packet_data_t data = {
        .start_flag = START_FLAG,
    };

    node_send_status_t status;
    esp_err_t err;

    for (;;) {
        err = node_broadcast((const uint8_t *)&data, sizeof(packet_data_t), &status, WAIT_NOTIFICATION);
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
