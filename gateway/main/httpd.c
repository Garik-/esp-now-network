#include "httpd.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "esp_check.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "mbedtls/base64.h"

#include "config.h"
#include "settings.h"

static const char *const TAG = "httpd";

static esp_err_t send_text(httpd_req_t *req, const char *text) {
    httpd_resp_set_type(req, "text/plain; charset=utf-8");
    return httpd_resp_send(req, text, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t send_unauthorized(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t require_basic_auth(httpd_req_t *req) {
    const char *user = GATEWAY_HTTP_AUTH_USER;
    const char *pass = GATEWAY_HTTP_AUTH_PASSWORD;
    if (user[0] == '\0' && pass[0] == '\0') {
        return ESP_OK;
    }

    size_t hdr_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (hdr_len == 0 || hdr_len >= 128) {
        return send_unauthorized(req);
    }

    char hdr[128];
    if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) != ESP_OK) {
        return send_unauthorized(req);
    }

    const char *prefix = "Basic ";
    size_t prefix_len = strlen(prefix);
    if (strncmp(hdr, prefix, prefix_len) != 0) {
        return send_unauthorized(req);
    }

    const char *b64 = hdr + prefix_len;
    size_t b64_len = strlen(b64);
    unsigned char decoded[128];
    size_t decoded_len = 0;
    if (mbedtls_base64_decode(decoded, sizeof(decoded) - 1, &decoded_len, (const unsigned char *)b64, b64_len) != 0) {
        return send_unauthorized(req);
    }
    decoded[decoded_len] = '\0';

    char expected[128];
    if (snprintf(expected, sizeof(expected), "%s:%s", user, pass) <= 0) {
        return send_unauthorized(req);
    }

    if (strcmp((const char *)decoded, expected) != 0) {
        return send_unauthorized(req);
    }

    return ESP_OK;
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

    if (require_basic_auth(req) != ESP_OK) {
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

    if (require_basic_auth(req) != ESP_OK) {
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

    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handle_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "no-cache, must-revalidate");

    extern const unsigned char index_html_start[] asm("_binary_index_html_gz_start");
    extern const unsigned char index_html_end[] asm("_binary_index_html_gz_end");
    const size_t index_html_size = (index_html_end - index_html_start);

    return httpd_resp_send(req, (const char *)index_html_start, index_html_size);
}

static esp_err_t handle_auth_check(httpd_req_t *req) {
    if (require_basic_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t handle_settings_csv(httpd_req_t *req) {
    if (require_basic_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    char buf[512];
    size_t out_size = 0;
    esp_err_t err = settings_to_csv(buf, sizeof(buf), &out_size);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "csv failed");
        return err;
    }

    httpd_resp_set_type(req, "text/csv; charset=utf-8");
    return httpd_resp_send(req, buf, out_size);
}

static esp_err_t register_config_endpoint(httpd_handle_t server, const char *uri, const char *key) {
    /*
    httpd_uri_t get_uri = {
        .uri = uri,
        .method = HTTP_GET,
        .handler = handle_get,
        .user_ctx = (void *)key,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &get_uri), TAG, "httpd_register_uri_handler");
    */

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

    httpd_uri_t auth_check = {
        .uri = "/auth/check",
        .method = HTTP_GET,
        .handler = handle_auth_check,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &auth_check), TAG, "httpd_register_uri_handler");

    httpd_uri_t settings_csv = {
        .uri = "/settings.csv",
        .method = HTTP_GET,
        .handler = handle_settings_csv,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_csv), TAG, "httpd_register_uri_handler");

    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/wifi/ssid", "wifi.ssid"), TAG, "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/wifi/password", "wifi.password"), TAG,
                        "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/mqtt/uri", "mqtt.uri"), TAG, "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/mqtt/user", "mqtt.user"), TAG, "register_endpoint");
    ESP_RETURN_ON_ERROR(register_config_endpoint(server, "/config/mqtt/password", "mqtt.password"), TAG,
                        "register_endpoint");

    ESP_LOGI(TAG, "HTTP server started, port=%d auth.user=%s auth.password=%s", config.server_port,
             GATEWAY_HTTP_AUTH_USER, GATEWAY_HTTP_AUTH_PASSWORD);
    return ESP_OK;
}
