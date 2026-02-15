#ifndef _WIFI_H_
#define _WIFI_H_

#include "closer.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WAIT_STA_GOT_IP_MAX_MS 10000 // TODO: make configurable

/**
 * @brief Initializes Wi-Fi stack and starts station/AP operation.
 *
 * Registers cleanup handlers into provided closer.
 *
 * @param closer Closer handle used for deferred cleanup registration.
 * @param arg Reserved user argument (currently unused).
 * @return ESP_OK on success, or an error code on initialization failure.
 */
esp_err_t wifi_start(closer_handle_t closer, void *arg);

#ifdef __cplusplus
}
#endif

#endif /* _WIFI_H_ */
