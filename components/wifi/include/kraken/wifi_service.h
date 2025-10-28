#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_SSID_MAX_LEN 32
#define WIFI_PASSWORD_MAX_LEN 64
#define WIFI_MAX_SCAN_RESULTS 20

typedef struct {
    char ssid[WIFI_SSID_MAX_LEN];
    int8_t rssi;
    uint8_t auth_mode;
    uint8_t channel;
} wifi_ap_info_t;

typedef struct {
    wifi_ap_info_t aps[WIFI_MAX_SCAN_RESULTS];
    uint16_t count;
} wifi_scan_result_t;

esp_err_t wifi_service_init(void);
esp_err_t wifi_service_deinit(void);

esp_err_t wifi_service_enable(void);
esp_err_t wifi_service_disable(void);
bool wifi_service_is_enabled(void);

esp_err_t wifi_service_scan(void);
esp_err_t wifi_service_get_scan_results(wifi_scan_result_t *results);

esp_err_t wifi_service_connect(const char *ssid, const char *password);
esp_err_t wifi_service_disconnect(void);
bool wifi_service_is_connected(void);

esp_err_t wifi_service_get_ip(char *ip_str, size_t len);

#ifdef __cplusplus
}
#endif
