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

#define AUTH_PLAIN_MAX_LEN 128
#define AUTH_HDR_MAX_LEN 192
#define SETTINGS_CSV_MAX_LEN 512

static char s_expected_auth_hdr[AUTH_HDR_MAX_LEN];
static size_t s_expected_auth_hdr_len = 0;

static esp_err_t build_expected_auth_hdr(const char *user, const char *password) {
    if (unlikely(user == NULL || password == NULL || user[0] == '\0' || password[0] == '\0')) {
        ESP_LOGE(TAG, "HTTP auth user/password must be non-empty");
        return ESP_ERR_INVALID_ARG;
    }

    char plain[AUTH_PLAIN_MAX_LEN];
    if (snprintf(plain, sizeof(plain), "%s:%s", user, password) <= 0) {
        ESP_LOGE(TAG, "Failed to format auth credentials");
        return ESP_FAIL;
    }

    const char *prefix = "Basic ";
    size_t prefix_len = strlen(prefix);
    memcpy(s_expected_auth_hdr, prefix, prefix_len);

    size_t b64_out_len = 0;
    int rc = mbedtls_base64_encode((unsigned char *)s_expected_auth_hdr + prefix_len,
                                   sizeof(s_expected_auth_hdr) - prefix_len - 1, &b64_out_len,
                                   (const unsigned char *)plain, strlen(plain));
    if (rc != 0) {
        ESP_LOGE(TAG, "Failed to base64 encode auth credentials");
        return ESP_FAIL;
    }

    s_expected_auth_hdr_len = prefix_len + b64_out_len;
    s_expected_auth_hdr[s_expected_auth_hdr_len] = '\0';
    return ESP_OK;
}

static esp_err_t send_unauthorized(httpd_req_t *req) {
    httpd_resp_set_status(req, "401 Unauthorized");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t require_basic_auth(httpd_req_t *req) {
    if (s_expected_auth_hdr_len == 0) {
        return send_unauthorized(req);
    }

    size_t hdr_len = httpd_req_get_hdr_value_len(req, "Authorization");
    if (hdr_len == 0 || hdr_len != s_expected_auth_hdr_len || hdr_len >= sizeof(s_expected_auth_hdr)) {
        return send_unauthorized(req);
    }

    char hdr[AUTH_HDR_MAX_LEN];
    if (httpd_req_get_hdr_value_str(req, "Authorization", hdr, sizeof(hdr)) != ESP_OK) {
        return send_unauthorized(req);
    }

    if (strcmp(hdr, s_expected_auth_hdr) != 0) {
        return send_unauthorized(req);
    }

    return ESP_OK;
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

    char buf[SETTINGS_CSV_MAX_LEN];
    size_t out_size = 0;
    esp_err_t err = settings_to_csv(buf, sizeof(buf), &out_size);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "csv failed");
        return err;
    }

    httpd_resp_set_type(req, "text/csv; charset=utf-8");
    return httpd_resp_send(req, buf, out_size);
}

static esp_err_t handle_settings_csv_post(httpd_req_t *req) {
    if (require_basic_auth(req) != ESP_OK) {
        return ESP_FAIL;
    }

    if (req->content_len == 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "empty body");
        return ESP_FAIL;
    }

    char buf[SETTINGS_CSV_MAX_LEN];
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

    esp_err_t err = settings_parse_from_csv(buf, read);
    if (err == ESP_ERR_INVALID_ARG) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "invalid csv");
        return ESP_FAIL;
    }
    if (err == ESP_ERR_INVALID_SIZE) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "value too large");
        return ESP_FAIL;
    }
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "save failed");
        return err;
    }

    return httpd_resp_send(req, NULL, 0);
}

esp_err_t httpd_start_server(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = GATEWAY_HTTP_PORT;

    ESP_RETURN_ON_ERROR(build_expected_auth_hdr(GATEWAY_HTTP_AUTH_USER, GATEWAY_HTTP_AUTH_PASSWORD), TAG,
                        "build_expected_auth_hdr");

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

    httpd_uri_t settings_csv_post = {
        .uri = "/settings.csv",
        .method = HTTP_POST,
        .handler = handle_settings_csv_post,
        .user_ctx = NULL,
    };
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(server, &settings_csv_post), TAG, "httpd_register_uri_handler");

    ESP_LOGI(TAG, "HTTP server started, port=%d auth.user=%s auth.password=%s", config.server_port,
             GATEWAY_HTTP_AUTH_USER, GATEWAY_HTTP_AUTH_PASSWORD);
    return ESP_OK;
}
