#include "settings.h"

#include <stdbool.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"

#include "config.h"

static const char *const TAG = "settings";

#define SETTINGS_NAMESPACE "cfg"

typedef struct {
    const char *key;
    char *buf;
    size_t buf_len;
} setting_entry_t;

static settings_t s_settings;
static bool s_settings_loaded = false;

static setting_entry_t s_entries[] = {
    {.key = "wifi.ssid", .buf = s_settings.wifi_ssid, .buf_len = sizeof(s_settings.wifi_ssid)},
    {.key = "wifi.password", .buf = s_settings.wifi_password, .buf_len = sizeof(s_settings.wifi_password)},
    {.key = "mqtt.uri", .buf = s_settings.mqtt_uri, .buf_len = sizeof(s_settings.mqtt_uri)},
    {.key = "mqtt.user", .buf = s_settings.mqtt_user, .buf_len = sizeof(s_settings.mqtt_user)},
    {.key = "mqtt.password", .buf = s_settings.mqtt_password, .buf_len = sizeof(s_settings.mqtt_password)},
};

static void settings_apply_defaults(settings_t *out) {
    strlcpy(out->wifi_ssid, GATEWAY_WIFI_SSID, sizeof(out->wifi_ssid));
    strlcpy(out->wifi_password, GATEWAY_WIFI_PASSWORD, sizeof(out->wifi_password));
    strlcpy(out->mqtt_uri, GATEWAY_BROKER_URL, sizeof(out->mqtt_uri));
    strlcpy(out->mqtt_user, GATEWAY_BROKER_USERNAME, sizeof(out->mqtt_user));
    strlcpy(out->mqtt_password, GATEWAY_BROKER_PASSWORD, sizeof(out->mqtt_password));
}

static setting_entry_t *settings_find_entry(const char *key) {
    for (size_t i = 0; i < sizeof(s_entries) / sizeof(s_entries[0]); i++) {
        if (strcmp(s_entries[i].key, key) == 0) {
            return &s_entries[i];
        }
    }

    return NULL;
}

#define SETTINGS_MAX_VALUE_LEN (sizeof(s_settings.mqtt_uri))

static setting_entry_t *settings_find_entry_len(const char *key, size_t key_len) {
    for (size_t i = 0; i < sizeof(s_entries) / sizeof(s_entries[0]); i++) {
        const char *entry_key = s_entries[i].key;
        if (strlen(entry_key) == key_len && memcmp(entry_key, key, key_len) == 0) {
            return &s_entries[i];
        }
    }

    return NULL;
}

esp_err_t settings_to_csv(char *out, size_t out_len, size_t *out_size) {
    if (out == NULL || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    const settings_t *s = settings_get();

    int n = snprintf(out, out_len,
                     "wifi.ssid=%s\n"
                     "wifi.password=%s\n"
                     "mqtt.uri=%s\n"
                     "mqtt.user=%s\n"
                     "mqtt.password=%s\n",
                     s->wifi_ssid, s->wifi_password, s->mqtt_uri, s->mqtt_user, s->mqtt_password);

    if (n < 0) {
        return ESP_FAIL;
    }
    if ((size_t)n >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }
    if (out_size != NULL) {
        *out_size = (size_t)n;
    }
    return ESP_OK;
}

esp_err_t settings_parse_from_csv(const char *csv, size_t csv_len) {
    if (csv == NULL || csv_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_settings_loaded) {
        settings_init();
    }

    const char *p = csv;
    size_t remaining = csv_len;

    char value_buf[SETTINGS_MAX_VALUE_LEN];

    while (remaining > 0) {
        const char *line_end = memchr(p, '\n', remaining);
        size_t line_len = line_end ? (size_t)(line_end - p) : remaining;
        if (line_len > 0 && p[line_len - 1] == '\r') {
            line_len--;
        }
        if (line_len == 0) {
            if (line_end == NULL) {
                break;
            }
            p = line_end + 1;
            remaining = csv_len - (size_t)(p - csv);
            continue;
        }

        const char *eq = memchr(p, '=', line_len);
        if (eq == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        size_t key_len = (size_t)(eq - p);
        size_t val_len = line_len - key_len - 1;
        if (key_len == 0) {
            return ESP_ERR_INVALID_ARG;
        }

        setting_entry_t *entry = settings_find_entry_len(p, key_len);
        if (entry == NULL) {
            return ESP_ERR_INVALID_ARG;
        }
        if (entry->buf_len > SETTINGS_MAX_VALUE_LEN || val_len >= entry->buf_len) {
            return ESP_ERR_INVALID_SIZE;
        }

        memcpy(value_buf, eq + 1, val_len);
        value_buf[val_len] = '\0';

        esp_err_t err = settings_set(entry->key, value_buf);
        if (err != ESP_OK) {
            return err;
        }

        if (line_end == NULL) {
            break;
        }
        p = line_end + 1;
        remaining = csv_len - (size_t)(p - csv);
    }

    return ESP_OK;
}

static esp_err_t settings_load_from_nvs(void) {
    nvs_handle_t nvs = 0;
    esp_err_t err = nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &nvs);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return ESP_OK;
    }
    if (err != ESP_OK) {
        return err;
    }

    for (size_t i = 0; i < sizeof(s_entries) / sizeof(s_entries[0]); i++) {
        setting_entry_t *entry = &s_entries[i];
        size_t len = entry->buf_len;
        err = nvs_get_str(nvs, entry->key, entry->buf, &len);
        if (err == ESP_ERR_NVS_NOT_FOUND) {
            continue;
        }
        if (err != ESP_OK) {
            nvs_close(nvs);
            return err;
        }
    }

    nvs_close(nvs);
    return ESP_OK;
}

esp_err_t settings_init(void) {
    if (s_settings_loaded) {
        return ESP_OK;
    }

    settings_apply_defaults(&s_settings);

    esp_err_t err = settings_load_from_nvs();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to load settings from NVS: %s", esp_err_to_name(err));
        return err;
    }

    s_settings_loaded = true;
    return ESP_OK;
}

const settings_t *settings_get(void) {
    if (!s_settings_loaded) {
        settings_init();
    }
    return &s_settings;
}

const char *settings_wifi_ssid(void) {
    return settings_get()->wifi_ssid;
}

const char *settings_wifi_password(void) {
    return settings_get()->wifi_password;
}

const char *settings_mqtt_uri(void) {
    return settings_get()->mqtt_uri;
}

const char *settings_mqtt_user(void) {
    return settings_get()->mqtt_user;
}

const char *settings_mqtt_password(void) {
    return settings_get()->mqtt_password;
}

const char *settings_get_value(const char *key) {
    setting_entry_t *entry = settings_find_entry(key);
    if (entry == NULL) {
        return NULL;
    }
    return entry->buf;
}

esp_err_t settings_set(const char *key, const char *value) {
    if (key == NULL || value == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_settings_loaded) {
        settings_init();
    }

    setting_entry_t *entry = settings_find_entry(key);
    if (entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs = 0;
    ESP_RETURN_ON_ERROR(nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs_open");
    esp_err_t err = nvs_set_str(nvs, key, value);
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);

    if (err != ESP_OK) {
        return err;
    }

    strlcpy(entry->buf, value, entry->buf_len);
    return ESP_OK;
}

esp_err_t settings_clear(const char *key) {
    if (key == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_settings_loaded) {
        settings_init();
    }

    setting_entry_t *entry = settings_find_entry(key);
    if (entry == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t nvs = 0;
    ESP_RETURN_ON_ERROR(nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &nvs), TAG, "nvs_open");
    esp_err_t err = nvs_erase_key(nvs, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        err = ESP_OK;
    }
    if (err == ESP_OK) {
        err = nvs_commit(nvs);
    }
    nvs_close(nvs);
    if (err != ESP_OK) {
        return err;
    }

    settings_apply_defaults(&s_settings);
    settings_load_from_nvs();
    return ESP_OK;
}
