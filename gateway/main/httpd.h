#ifndef _HTTPD_H_
#define _HTTPD_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Starts embedded HTTP server and registers all URI handlers.
 *
 * @return ESP_OK on success, or an error code when server setup fails.
 */
esp_err_t httpd_start_server(void);

#ifdef __cplusplus
}
#endif

#endif /* _HTTPD_H_ */
