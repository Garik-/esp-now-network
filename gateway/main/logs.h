#ifndef _LOGS_H_
#define _LOGS_H_

#include "esp_err.h"
#include "freertos/ringbuf.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initializes log ring buffer.
 *
 * @param[out] out Pointer to receive created ring buffer handle.
 * @return ESP_OK on success, ESP_ERR_INVALID_STATE if already initialized,
 *         or ESP_ERR_NO_MEM on allocation failure.
 */
esp_err_t logs_init(RingbufHandle_t *out);

/**
 * @brief Deinitializes log ring buffer and releases its resources.
 *
 * @return ESP_OK on success, or ESP_ERR_INVALID_STATE if not initialized.
 */
esp_err_t logs_deinit(void);

/**
 * @brief Appends a log record to ring buffer.
 *
 * If logging is not initialized, message length is zero, or the ring buffer
 * is full, the message is silently dropped.
 *
 * @param msg Pointer to message bytes.
 * @param len Message length in bytes.
 * @return ESP_OK on success or when dropped silently, ESP_ERR_INVALID_ARG for
 *         invalid input (NULL message with non-zero length).
 */
esp_err_t logs_push(const char *msg, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* _LOGS_H_ */
