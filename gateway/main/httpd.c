#include "httpd.h"

#include <ctype.h>
#include <string.h>

#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"

#include "config.h"
#include "settings.h"

static const char *const TAG = "httpd";

static esp_err_t send_text(httpd_req_t *req, const char *text) {
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
}

static char *trim_whitespace(char *s) {
    if (s == NULL) {
        return s;
    }

    while (*s && isspace((unsigned char)*s)) {
        s++;
    }

    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }

    return s;
}

static esp_err_t handle_get(httpd_req_t *req) {
    const char *key = (const char *)req->user_ctx;
    if (key == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "missing key");
        return ESP_FAIL;
    }

    if (strcmp(key, "wifi.password") == 0 || strcmp(key, "mqtt.password") == 0) {
        httpd_resp_send_err(req, HTTPD_403_FORBIDDEN, "password read not allowed");
        return ESP_FAIL;
    }

    const char *value = settings_get_value(key);
    if (value == NULL) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "unknown key");
        return ESP_FAIL;
    }

    return send_text(req, value);
}

static esp_err_t handle_post(httpd_req_t *req) {
    const char *key = (const char *)req->user_ctx;
    if (key == NULL) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "missing key");
        return ESP_FAIL;
    }

    if (req->content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }

    char buf[160];
    size_t to_read = req->content_len;
    if (to_read >= sizeof(buf)) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "body too large");
        return ESP_FAIL;
    }

    size_t read = 0;
    while (read < to_read) {
        int r = httpd_req_recv(req, buf + read, to_read - read);
        if (r <= 0) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "recv failed");
            return ESP_FAIL;
        }
        read += (size_t)r;
    }
    buf[read] = '\0';

    char *value = trim_whitespace(buf);
    if (value[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty value");
        return ESP_FAIL;
    }

    esp_err_t err = settings_set(key, value);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
        return err;
    }

    return send_text(req, "ok");
}

static esp_err_t handle_root(httpd_req_t *req) {
    const char *body = "Gateway config endpoints:\n"
                       "GET  /config/wifi/ssid\n"
                       "POST /config/wifi/ssid\n"
                       "POST /config/wifi/password\n"
                       "GET  /config/mqtt/uri\n"
                       "POST /config/mqtt/uri\n"
                       "GET  /config/mqtt/user\n"
                       "POST /config/mqtt/user\n"
                       "POST /config/mqtt/password\n";

    return send_text(req, body);
}

static esp_err_t register_config_endpoint(httpd_handle_t server, const char *uri, const char *key) {
    httpd_uri_t get_uri = {
        .uri = uri,
        .method = HTTP_GET,
        .handler = handle_get,
        .user_ctx = (void *)key,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_uri), TAG, "httpd_register_uri_handler");

    httpd_uri_t post_uri = {
        .uri = uri,
        .method = HTTP_POST,
        .handler = handle_post,
        .user_ctx = (void *)key,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &post_uri), TAG, "httpd_register_uri_handler");

    return ESP_OK;
}

esp_err_t httpd_start_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = GATEWAY_HTTP_PORT;
    config.max_uri_handlers = 16;

    httpd_handle_t server = NULL;
    ESP_RETURN_ON_ERROR(httpd_start(&server, &config), TAG, "httpd_start");

    httpd_uri_t root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = handle_root,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &root), TAG, "httpd_register_uri_handler");

    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/wifi/ssid", "wifi.ssid"), TAG, "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/wifi/password", "wifi.password"), TAG,
                        "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/mqtt/uri", "mqtt.uri"), TAG, "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/mqtt/user", "mqtt.user"), TAG, "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/mqtt/password", "mqtt.password"), TAG,
                        "register_endpoint");

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);
    return ESP_OK;
}
