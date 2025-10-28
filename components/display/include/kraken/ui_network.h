#pragma once

#include "lvgl.h"
#include "kraken/kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the network settings screen
 * @param parent Parent LVGL object to attach the screen to
 * @return Created network screen object
 */
lv_obj_t *ui_network_screen_create(lv_obj_t *parent);

/**
 * @brief Show the network settings screen
 */
void ui_network_screen_show(void);

/**
 * @brief Hide the network settings screen
 */
void ui_network_screen_hide(void);

/**
 * @brief Update the network list after WiFi scan completes
 * Call this when KRAKEN_EVENT_WIFI_SCAN_DONE is received
 */
void ui_network_update_scan_results(void);

/**
 * @brief Handle input events for network screen navigation
 * @param input Input event type
 */
void ui_network_handle_input(kraken_event_type_t input);

/**
 * @brief Handle WiFi connected event
 * Call this when KRAKEN_EVENT_WIFI_CONNECTED is received
 */
void ui_network_on_wifi_connected(void);

/**
 * @brief Handle WiFi disconnected event
 * @param was_connecting True if this was during a connection attempt (shows "Failed")
 * Call this when KRAKEN_EVENT_WIFI_DISCONNECTED is received
 */
void ui_network_on_wifi_disconnected(bool was_connecting);

/**
 * @brief Show a notification message
 * @param message Message text to display
 * @param duration_ms Auto-dismiss duration in milliseconds (0 = no auto-dismiss)
 */
void ui_network_show_notification(const char *message, uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
