/**
 * @file closer.h
 * @brief Defer-style resource cleanup utility for C (ESP-IDF compatible)
 *
 * This header defines a "closer" abstraction, allowing registration of
 * cleanup functions that are called in reverse order of registration,
 * similar to Go's "defer".
 *
 * @author garik.djan <garik.djan@gmail.com>
 * @version 0.0.4
 */

#ifndef _CLOSER_H_
#define _CLOSER_H_

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Type of cleanup function to register with a closer.
 */
typedef esp_err_t (*closer_fn_t)(void);

/**
 * @brief Opaque handle to a closer.
 */
typedef struct closer_t *closer_handle_t;

typedef esp_err_t (*with_closer_fn_t)(closer_handle_t, void *arg);

/**
 * @brief Creates a new closer object.
 *
 * @param[out] out Pointer to a variable to receive the handle.
 * @return ESP_OK on success,
 *         ESP_ERR_INVALID_ARG if out is NULL,
 *         ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t closer_create(closer_handle_t *out);

/**
 * @brief Destroys a closer and frees all associated memory.
 *
 * @param h Handle to the closer.
 */
void closer_destroy(closer_handle_t h);

/**
 * @brief Adds a cleanup function to the closer.
 *
 * Functions are called in reverse order of registration when closer_close()
 * or closer_destroy() is called.
 *
 * @param h Handle to the closer.
 * @param fn Cleanup function to add.
 * @return ESP_OK on success,
 *         ESP_ERR_INVALID_ARG if h or fn is NULL,
 *         ESP_ERR_NO_MEM if memory allocation fails.
 */
esp_err_t closer_add(closer_handle_t h, closer_fn_t fn, const char *what);

/**
 * @brief Calls all registered cleanup functions and clears the list.
 *
 * After calling closer_close(), the closer can still be used to register new functions.
 *
 * @param h Handle to the closer.
 */
void closer_close(closer_handle_t h);

esp_err_t with_closer(with_closer_fn_t fn, void *arg);

#define DEFER(call, closer, cleanup_fn)                                                                                \
    do {                                                                                                               \
        esp_err_t err_rc_ = (call);                                                                                    \
        if (err_rc_ != ESP_OK) {                                                                                       \
            ESP_LOGE(TAG, "%s failed: %s", #call, esp_err_to_name(err_rc_));                                           \
            return err_rc_;                                                                                            \
        }                                                                                                              \
        closer_add((closer), (cleanup_fn), #cleanup_fn);                                                               \
    } while (0)

#ifdef CLOSER_IMPLEMENTATION

typedef struct closer_item {
    closer_fn_t fn;
    const char *what;
    struct closer_item *next;
} closer_item_t;

struct closer_t {
    closer_item_t *top;
};

esp_err_t closer_create(closer_handle_t *out) {
    if (unlikely(!out))
        return ESP_ERR_INVALID_ARG;

    struct closer_t *c = calloc(1, sizeof(*c));
    if (unlikely(!c))
        return ESP_ERR_NO_MEM;

    *out = c;
    return ESP_OK;
}

void closer_destroy(closer_handle_t h) {
    if (unlikely(!h)) {
        ESP_LOGW(TAG, "closer_destroy called with NULL handle");
        return;
    }

    closer_item_t *item = h->top;
    while (item) {
        closer_item_t *tmp = item;
        item = item->next;
        free(tmp);
    }

    free(h);
}

esp_err_t closer_add(closer_handle_t h, closer_fn_t fn, const char *what) {
    if (unlikely(!h || !fn))
        return ESP_ERR_INVALID_ARG;

    closer_item_t *item = malloc(sizeof(*item));
    if (unlikely(!item))
        return ESP_ERR_NO_MEM;

    item->fn = fn;
    item->what = what;
    item->next = h->top;
    h->top = item;

    return ESP_OK;
}

void closer_close(closer_handle_t h) {
    if (unlikely(!h))
        return;

    esp_err_t first_err = ESP_OK;
    closer_item_t *item = h->top;
    while (item) {
        esp_err_t err = item->fn();
        if (err != ESP_OK && first_err == ESP_OK) {
            first_err = err;
            ESP_LOGE(TAG, "closer failed: %s (%s)", item->what ? item->what : "<unknown>", esp_err_to_name(err));
        }

        closer_item_t *tmp = item;
        item = item->next;
        free(tmp);
    }

    h->top = NULL;
}

esp_err_t with_closer(with_closer_fn_t fn, void *arg) {
    esp_err_t err;
    closer_handle_t closer = NULL;

    err = closer_create(&closer);
    if (unlikely(err != ESP_OK))
        return err;

    err = fn(closer, arg);
    if (err != ESP_OK) {
        closer_close(closer);
    }

    closer_destroy(closer);

    return err;
}

#endif /* CLOSER_IMPLEMENTATION */

#ifdef __cplusplus
}
#endif

#endif /* _CLOSER_H_ */