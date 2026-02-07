#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "mdns.h"
#include "nvs_flash.h"

static const char *TAG = "main_gateway";
static httpd_handle_t s_server = NULL;

#define CLOSER_IMPLEMENTATION
#include "closer.h"

#include "config.h"
#include "espnow.h"
#include "wifi.h"

__attribute__((cold)) static esp_err_t nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (unlikely(ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs_flash_erase");
        ret = nvs_flash_init();
    }

    return ret;
}

__attribute__((cold)) static esp_err_t mdns_start() { // TODO: add error handling
    ESP_RETURN_ON_ERROR(mdns_init(), TAG, "mdns_init failed");
    // DEFER(mdns_free);

    ESP_RETURN_ON_ERROR(mdns_hostname_set(GATEWAY_MDNS_NAME), TAG, "mdns_hostname_set");
    ESP_LOGI(TAG, "mdns hostname set to: [%s]", GATEWAY_MDNS_NAME);

    ESP_RETURN_ON_ERROR(mdns_instance_name_set(GATEWAY_MDNS_INSTANCE_NAME), TAG,
                        "mdns_instance_name_set"); // TODO: make configurable
    ESP_RETURN_ON_ERROR(mdns_service_add(NULL, "_http", "_tcp", GATEWAY_HTTP_PORT, NULL, 0), TAG, "mdns_service_add");

    return ESP_OK;
}

static esp_err_t api_channel_get_handler(httpd_req_t *req) {

    uint8_t channel = wifi_get_channel();

    char resp[4];
    int len = snprintf(resp, sizeof(resp), "%d", channel);
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, resp, len);

    return ESP_OK;
}

static esp_err_t start_webserver() {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = GATEWAY_HTTP_PORT;

    config.lru_purge_enable = true;
    config.max_open_sockets = 4;

    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    config.keep_alive_enable = true;

    config.stack_size = 6144;
    config.max_uri_handlers = 8;

    config.task_priority = tskIDLE_PRIORITY + 3;

    ESP_LOGI(TAG, "starting server on port: '%d'", config.server_port);
    ESP_RETURN_ON_ERROR(httpd_start(&s_server, &config), TAG, "httpd_start failed"); // TODO: add error handling
    // DEFER(stop_webserver);

    static const httpd_uri_t api_channel_get = {
        .uri = "/api/channel", .method = HTTP_GET, .handler = api_channel_get_handler}; // TODO: need static?
    ESP_RETURN_ON_ERROR(httpd_register_uri_handler(s_server, &api_channel_get), TAG, "httpd register /api/channel");

    return ESP_OK;
}

espnow_rx_handler_t handle(const espnow_rx_t *rx) {
    char mac_str[27];
    snprintf(mac_str, sizeof(mac_str), "/device/" MACSTR "", MAC2STR(rx->mac_addr));

    ESP_LOGI(TAG, "received data from %s, len: %d", mac_str, rx->len); // TODO: delete
    return ESP_OK;
}

esp_err_t app_run() {
    ESP_RETURN_ON_ERROR(nvs_init(), TAG, "nvs_init");
    ESP_RETURN_ON_ERROR(with_closer(wifi_start, NULL), TAG, "wifi_start");
    ESP_RETURN_ON_ERROR(with_closer(espnow_start, &handle), TAG, "espnow_start");
    ESP_RETURN_ON_ERROR(wifi_connect(), TAG, "wifi_connect");
    ESP_RETURN_ON_ERROR(mdns_start(), TAG, "mdns_start");
    ESP_RETURN_ON_ERROR(start_webserver(), TAG, "httpd_start");

    return ESP_OK;
}

void app_main(void) {
    ESP_ERROR_CHECK(app_run());
}
