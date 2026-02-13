#include "settings.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_check.h"
#include "esp_log.h"
#include "nvs.h"

#include "config.h"

static const char *const TAG = "settings";

#define SETTINGS_NAMESPACE "cfg"
#define SETTINGS_MAX_VALUE_LEN (sizeof(s_settings.mqtt_uri))

typedef enum {
    SETTING_TYPE_STR = 0,
    SETTING_TYPE_U8,
} setting_type_t;

typedef union {
    char *str;
    uint8_t *u8;
} setting_value_ptr_t;

typedef struct {
    const char *key;
    setting_type_t type;
    setting_value_ptr_t value;
    size_t str_buf_len;
} setting_entry_t;

#define SETTING_ENTRY_STR(k, b) {.key = (k), .type = SETTING_TYPE_STR, .value.str = (b), .str_buf_len = sizeof(b)}

#define SETTING_ENTRY_U8(k, b) {.key = (k), .type = SETTING_TYPE_U8, .value.u8 = (b), .str_buf_len = 0}

static settings_t s_settings;
static bool s_settings_loaded = false;

static setting_entry_t s_entries[] = {
    SETTING_ENTRY_STR("wifi.ssid", s_settings.wifi_ssid),
    SETTING_ENTRY_STR("wifi.password", s_settings.wifi_password),
    SETTING_ENTRY_U8("wifi.channel", &s_settings.wifi_channel),
    SETTING_ENTRY_STR("http.auth.user", s_settings.http_auth_user),
    SETTING_ENTRY_STR("http.auth.password", s_settings.http_auth_password),
    SETTING_ENTRY_STR("mqtt.uri", s_settings.mqtt_uri),
    SETTING_ENTRY_STR("mqtt.user", s_settings.mqtt_user),
    SETTING_ENTRY_STR("mqtt.password", s_settings.mqtt_password),
};

static esp_err_t settings_parse_u8(const char *value, uint8_t *out) {
    if (value == NULL || out == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    char *endptr = NULL;
    long parsed = strtol(value, &endptr, 10);
    if (value == endptr || *endptr != '\0' || parsed < 0 || parsed > UINT8_MAX) {
        return ESP_ERR_INVALID_ARG;
    }

    *out = (uint8_t)parsed;
    return ESP_OK;
}

static void settings_apply_defaults(settings_t *out) {
    strlcpy(out->wifi_ssid, GATEWAY_WIFI_SSID, sizeof(out->wifi_ssid));
    strlcpy(out->wifi_password, GATEWAY_WIFI_PASSWORD, sizeof(out->wifi_password));
    out->wifi_channel = (uint8_t)GATEWAY_WIFI_CHANNEL_DEFAULT;
    strlcpy(out->http_auth_user, GATEWAY_HTTP_AUTH_USER, sizeof(out->http_auth_user));
    strlcpy(out->http_auth_password, GATEWAY_HTTP_AUTH_PASSWORD, sizeof(out->http_auth_password));
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

static setting_entry_t *settings_find_entry_len(const char *key, size_t key_len) {
    for (size_t i = 0; i < sizeof(s_entries) / sizeof(s_entries[0]); i++) {
        const char *entry_key = s_entries[i].key;
        if (strlen(entry_key) == key_len && memcmp(entry_key, key, key_len) == 0) {
            return &s_entries[i];
        }
    }

    return NULL;
}

static esp_err_t settings_entry_to_string(const setting_entry_t *entry, char *out, size_t out_len) {
    if (entry == NULL || out == NULL || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    int n = 0;
    switch (entry->type) {
    case SETTING_TYPE_STR:
        n = snprintf(out, out_len, "%s", entry->value.str);
        break;
    case SETTING_TYPE_U8:
        n = snprintf(out, out_len, "%u", *entry->value.u8);
        break;
    default:
        return ESP_ERR_INVALID_STATE;
    }

    if (n < 0) {
        return ESP_FAIL;
    }
    if ((size_t)n >= out_len) {
        return ESP_ERR_INVALID_SIZE;
    }

    return ESP_OK;
}

esp_err_t settings_to_csv(char *out, size_t out_len, size_t *out_size) {
    if (out == NULL || out_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_settings_loaded) {
        ESP_RETURN_ON_ERROR(settings_init(), TAG, "settings_init");
    }

    size_t used = 0;
    for (size_t i = 0; i < sizeof(s_entries) / sizeof(s_entries[0]); i++) {
        char value_buf[SETTINGS_MAX_VALUE_LEN];
        esp_err_t err = settings_entry_to_string(&s_entries[i], value_buf, sizeof(value_buf));
        if (err != ESP_OK) {
            return err;
        }

        int n = snprintf(out + used, out_len - used, "%s=%s\n", s_entries[i].key, value_buf);
        if (n < 0) {
            return ESP_FAIL;
        }
        if ((size_t)n >= (out_len - used)) {
            return ESP_ERR_INVALID_SIZE;
        }
        used += (size_t)n;
    }

    if (out_size != NULL) {
        *out_size = used;
    }

    return ESP_OK;
}

esp_err_t settings_parse_from_csv(const char *csv, size_t csv_len) {
    if (csv == NULL || csv_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_settings_loaded) {
        ESP_RETURN_ON_ERROR(settings_init(), TAG, "settings_init");
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
        if (val_len >= sizeof(value_buf)) {
            return ESP_ERR_INVALID_SIZE;
        }
        if (entry->type == SETTING_TYPE_STR && val_len >= entry->str_buf_len) {
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
        switch (entry->type) {
        case SETTING_TYPE_STR: {
            size_t len = entry->str_buf_len;
            err = nvs_get_str(nvs, entry->key, entry->value.str, &len);
            break;
        }
        case SETTING_TYPE_U8:
            err = nvs_get_u8(nvs, entry->key, entry->value.u8);
            break;
        default:
            nvs_close(nvs);
            return ESP_ERR_INVALID_STATE;
        }

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

uint8_t settings_wifi_channel(void) {
    return settings_get()->wifi_channel;
}

const char *settings_http_auth_user(void) {
    return settings_get()->http_auth_user;
}

const char *settings_http_auth_password(void) {
    return settings_get()->http_auth_password;
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
    static char value_buf[4];

    setting_entry_t *entry = settings_find_entry(key);
    if (entry == NULL) {
        return NULL;
    }

    if (entry->type == SETTING_TYPE_STR) {
        return entry->value.str;
    }

    if (settings_entry_to_string(entry, value_buf, sizeof(value_buf)) != ESP_OK) {
        return NULL;
    }

    return value_buf;
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

    esp_err_t ret = ESP_OK;
    switch (entry->type) {
    case SETTING_TYPE_STR:
        if (strlen(value) >= entry->str_buf_len) {
            ret = ESP_ERR_INVALID_SIZE;
            goto out;
        }

        ret = nvs_set_str(nvs, key, value);
        if (ret == ESP_OK) {
            strlcpy(entry->value.str, value, entry->str_buf_len);
        }
        break;
    case SETTING_TYPE_U8: {
        uint8_t parsed = 0;
        ESP_GOTO_ON_ERROR(settings_parse_u8(value, &parsed), out, TAG, "invalid key");

        ret = nvs_set_u8(nvs, key, parsed);
        if (ret == ESP_OK) {
            *entry->value.u8 = parsed;
        }
        break;
    }
    default:
        ret = ESP_ERR_INVALID_STATE;
        goto out;
    }

    if (ret == ESP_OK) {
        ret = nvs_commit(nvs);
    }

out:
    nvs_close(nvs);
    return ret;
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
