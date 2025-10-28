#pragma once

#include "lvgl.h"
#include "kraken/kernel.h"
#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the top status bar
 * @param parent Parent LVGL object to attach the top bar to
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ui_topbar_init(lv_obj_t *parent);

/**
 * @brief Update the clock display
 * @param time Pointer to tm structure with current time
 */
void ui_topbar_update_time(struct tm *time);

/**
 * @brief Update WiFi icon state
 * @param connected True if WiFi is connected
 * @param rssi Signal strength in dBm (-50 = strong, -70 = weak)
 */
void ui_topbar_update_wifi(bool connected, int8_t rssi);

/**
 * @brief Update Bluetooth icon state
 * @param enabled True if Bluetooth is enabled
 * @param connected True if device is connected
 */
void ui_topbar_update_bluetooth(bool enabled, bool connected);

/**
 * @brief Update battery icon and percentage
 * @param percent Battery level (0-100)
 * @param charging True if battery is charging
 */
void ui_topbar_update_battery(uint8_t percent, bool charging);

#ifdef __cplusplus
}
#endif
