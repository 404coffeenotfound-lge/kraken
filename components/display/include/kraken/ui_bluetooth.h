#pragma once

#include "lvgl.h"
#include "kraken/kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the Bluetooth settings screen
 * @param parent Parent LVGL object to attach the screen to
 * @return Created Bluetooth screen object
 */
lv_obj_t *ui_bluetooth_screen_create(lv_obj_t *parent);

/**
 * @brief Show the Bluetooth settings screen
 */
void ui_bluetooth_screen_show(void);

/**
 * @brief Hide the Bluetooth settings screen
 */
void ui_bluetooth_screen_hide(void);

/**
 * @brief Update the device list after Bluetooth scan completes
 * Call this when KRAKEN_EVENT_BT_SCAN_DONE is received
 */
void ui_bluetooth_update_scan_results(void);

/**
 * @brief Handle input events for Bluetooth screen navigation
 * @param input Input event type
 */
void ui_bluetooth_handle_input(kraken_event_type_t input);

/**
 * @brief Handle Bluetooth connected event
 * Call this when KRAKEN_EVENT_BT_CONNECTED is received
 */
void ui_bluetooth_on_bt_connected(void);

/**
 * @brief Handle Bluetooth disconnected event
 * @param was_connecting True if this was during a connection attempt (shows "Failed")
 * Call this when KRAKEN_EVENT_BT_DISCONNECTED is received
 */
void ui_bluetooth_on_bt_disconnected(bool was_connecting);

/**
 * @brief Show a notification message
 * @param message Message text to display
 * @param duration_ms Auto-dismiss duration in milliseconds (0 = no auto-dismiss)
 */
void ui_bluetooth_show_notification(const char *message, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
