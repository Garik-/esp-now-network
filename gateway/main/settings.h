#pragma once

#include "esp_err.h"

// Runtime configuration stored in NVS (namespace: "cfg").
typedef struct {
    char wifi_ssid[33];
    char wifi_password[65];
    char mqtt_uri[129];
    char mqtt_user[65];
    char mqtt_password[65];
} settings_t;

esp_err_t settings_init(void);
const settings_t *settings_get(void);

const char *settings_wifi_ssid(void);
const char *settings_wifi_password(void);
const char *settings_mqtt_uri(void);
const char *settings_mqtt_user(void);
const char *settings_mqtt_password(void);

esp_err_t settings_set(const char *key, const char *value);
esp_err_t settings_clear(const char *key);
const char *settings_get_value(const char *key);
esp_err_t settings_to_csv(char *out, size_t out_len, size_t *out_size);
