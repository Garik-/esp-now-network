#ifndef __NODE_H__
#define __NODE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "esp_err.h"
#include "esp_now.h"
#include "freertos/FreeRTOS.h"

typedef esp_now_send_status_t node_send_status_t;

/**
 * @brief Initialize ESP-NOW node
 * @param channel WiFi channel (1-13)
 * @param mac Optional MAC address, NULL to use default
 * @return ESP_OK on success
 */
esp_err_t node_init(uint8_t channel, const uint8_t *mac);

/**
 * @brief Send unicast message
 * @param peer_addr MAC address of peer
 * @param data Payload data
 * @param len Payload length
 * @param out_status Send status (ESP_NOW_SEND_SUCCESS or ESP_NOW_SEND_FAIL)
 * @param xTicksToWait Timeout in FreeRTOS ticks
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t node_send(const uint8_t *peer_addr, const uint8_t *data, size_t len, esp_now_send_status_t *out_status,
                    TickType_t xTicksToWait);

/**
 * @brief Send broadcast message
 * @param data Payload data
 * @param len Payload length
 * @param out_status Send status (ESP_NOW_SEND_SUCCESS or ESP_NOW_SEND_FAIL)
 * @param xTicksToWait Timeout in FreeRTOS ticks
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t node_broadcast(const uint8_t *data, size_t len, node_send_status_t *out_status, TickType_t xTicksToWait);

#ifdef __cplusplus
}
#endif

#endif /* __NODE_H__ */