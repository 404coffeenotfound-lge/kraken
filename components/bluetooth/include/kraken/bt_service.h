#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BT_DEVICE_NAME_MAX_LEN 64
#define BT_MAC_ADDR_LEN 6
#define BT_MAX_SCAN_RESULTS 20

// Device class/type detection
typedef enum {
    BT_DEVICE_TYPE_UNKNOWN = 0,
    BT_DEVICE_TYPE_PHONE,
    BT_DEVICE_TYPE_COMPUTER,
    BT_DEVICE_TYPE_HEADSET,
    BT_DEVICE_TYPE_SPEAKER,
    BT_DEVICE_TYPE_KEYBOARD,
    BT_DEVICE_TYPE_MOUSE,
    BT_DEVICE_TYPE_GAMEPAD,
    BT_DEVICE_TYPE_GPS,
    BT_DEVICE_TYPE_SERIAL,
} bt_device_type_t;

typedef struct {
    char name[BT_DEVICE_NAME_MAX_LEN];
    uint8_t mac[BT_MAC_ADDR_LEN];
    int8_t rssi;
    bt_device_type_t device_type;  // Detected device type
    uint32_t class_of_device;      // Bluetooth CoD (Class of Device)
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

esp_err_t bt_service_connect(const uint8_t *mac);
esp_err_t bt_service_disconnect(void);
bool bt_service_is_connected(void);

#ifdef __cplusplus
}
#endif
