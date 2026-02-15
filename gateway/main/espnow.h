#ifndef _ESPNOW_H_
#define _ESPNOW_H_

#include "closer.h"

#include "esp_err.h"
#include "esp_now.h"

#ifdef __cplusplus
extern "C" {
#endif

#define DATA_BUFFER_SIZE ESP_NOW_MAX_DATA_LEN

typedef struct {
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t data[DATA_BUFFER_SIZE];
    size_t len;
} espnow_rx_t;

/**
 * @brief Callback type for handling received ESP-NOW packets.
 *
 * @param rx Pointer to received packet data.
 * @return ESP_OK on success, or an error code if handling failed.
 */
typedef esp_err_t (*espnow_rx_handler_t)(const espnow_rx_t *rx);

/**
 * @brief Initializes ESP-NOW receive pipeline and starts background task.
 *
 * @param close Closer handle used to register cleanup routines.
 * @param arg User argument interpreted as @ref espnow_rx_handler_t callback.
 * @return ESP_OK on success, or an error code on initialization failure.
 */
esp_err_t espnow_start(closer_handle_t close, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _ESPNOW_H_ */
