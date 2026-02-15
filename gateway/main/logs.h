#include "esp_err.h"
#include "freertos/ringbuf.h"
#include <stddef.h>

esp_err_t logs_init(RingbufHandle_t *out);
esp_err_t logs_deinit(void);
esp_err_t logs_push(const char *msg, size_t len);
