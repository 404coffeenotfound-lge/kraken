#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#if CONFIG_BT_ENABLED
#include "esp_gap_ble_api.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BT_DEVICE_NAME_MAX_LEN 64
#define BT_MAC_ADDR_LEN 6
#define BT_MAX_SCAN_RESULTS 20

typedef struct {
    char name[BT_DEVICE_NAME_MAX_LEN];
    uint8_t mac[BT_MAC_ADDR_LEN];
    int8_t rssi;
#if CONFIG_BT_ENABLED
    esp_ble_addr_type_t addr_type;  // PUBLIC, RANDOM, etc.
#else
    uint8_t addr_type;  // Placeholder when BT is disabled
#endif
} bt_device_info_t;

typedef struct {
    bt_device_info_t devices[BT_MAX_SCAN_RESULTS];
    uint16_t count;
} bt_scan_result_t;

esp_err_t bt_service_init(void);
esp_err_t bt_service_deinit(void);

esp_err_t bt_service_enable(void);
esp_err_t bt_service_disable(void);
bool bt_service_is_enabled(void);

esp_err_t bt_service_scan(uint32_t duration_sec);
esp_err_t bt_service_get_scan_results(bt_scan_result_t *results);

#if CONFIG_BT_ENABLED
esp_err_t bt_service_connect(const uint8_t *mac, esp_ble_addr_type_t addr_type);
#else
esp_err_t bt_service_connect(const uint8_t *mac, uint8_t addr_type);
#endif
esp_err_t bt_service_disconnect(void);
bool bt_service_is_connected(void);

#ifdef __cplusplus
}
#endif
