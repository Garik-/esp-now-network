#ifndef _SETTINGS_H_
#define _SETTINGS_H_

#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Runtime configuration stored in NVS (namespace: "cfg").
typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    uint8_t wifi_channel;
    char http_auth_user[65];
    char http_auth_password[65];
    char mqtt_uri[129];
    char mqtt_user[65];
    char mqtt_password[65];
} settings_t;

/**
 * @brief Loads runtime settings from defaults and NVS.
 *
 * @return ESP_OK on success, or an error code if loading fails.
 */
esp_err_t settings_init(void);

/**
 * @brief Returns pointer to current settings snapshot.
 *
 * If settings are not initialized yet, initialization is performed lazily.
 *
 * @return Pointer to internal settings structure.
 */
const settings_t *settings_get(void);

/**
 * @brief Returns configured Wi-Fi SSID.
 */
const char *settings_wifi_ssid(void);

/**
 * @brief Returns configured Wi-Fi password.
 */
const char *settings_wifi_password(void);

/**
 * @brief Returns configured Wi-Fi channel.
 */
uint8_t settings_wifi_channel(void);

/**
 * @brief Returns HTTP basic auth username.
 */
const char *settings_http_auth_user(void);

/**
 * @brief Returns HTTP basic auth password.
 */
const char *settings_http_auth_password(void);

/**
 * @brief Returns MQTT broker URI.
 */
const char *settings_mqtt_uri(void);

/**
 * @brief Returns MQTT username.
 */
const char *settings_mqtt_user(void);

/**
 * @brief Returns MQTT password.
 */
const char *settings_mqtt_password(void);

/**
 * @brief Sets configuration value by key and persists it in NVS.
 *
 * @param key Setting key (e.g. "wifi.ssid").
 * @param value Setting value as string.
 * @return ESP_OK on success, or an error code if validation/persistence fails.
 */
esp_err_t settings_set(const char *key, const char *value);

/**
 * @brief Clears setting from NVS and restores defaults.
 *
 * @param key Setting key to clear.
 * @return ESP_OK on success, or an error code on failure.
 */
esp_err_t settings_clear(const char *key);

/**
 * @brief Returns setting value by key as string.
 *
 * @param key Setting key.
 * @return Pointer to value string, or NULL if key is unknown.
 */
const char *settings_get_value(const char *key);

/**
 * @brief Serializes settings into CSV lines in form "key=value\n".
 *
 * @param out Destination buffer.
 * @param out_len Destination buffer length.
 * @param[out] out_size Optional pointer to receive bytes written.
 * @return ESP_OK on success, or an error code if buffer is too small or on
 *         serialization failure.
 */
esp_err_t settings_to_csv(char *out, size_t out_len, size_t *out_size);

/**
 * @brief Parses CSV payload and applies settings.
 *
 * Expected line format: "key=value" separated by newline.
 *
 * @param csv Input CSV buffer.
 * @param csv_len Input size in bytes.
 * @return ESP_OK on success, or an error code on parse/validation failure.
 */
esp_err_t settings_parse_from_csv(const char *csv, size_t csv_len);

#ifdef __cplusplus
}
#endif

#endif /* _SETTINGS_H_ */
