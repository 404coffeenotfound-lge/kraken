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
 * @brief UI Status information
 * Contains current state of all status bar elements
 */
typedef struct {
    bool wifi_connected;
    int8_t wifi_rssi;
    bool bt_enabled;
    bool bt_connected;
    uint8_t battery_percent;
    bool battery_charging;
    struct tm current_time;
    bool time_synced;
} ui_status_t;

/**
 * @brief Initialize the UI manager
 * This initializes all UI components (topbar, menu, submenus)
 * @param screen The main LVGL screen object
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ui_manager_init(lv_obj_t *screen);

/**
 * @brief Deinitialize the UI manager
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ui_manager_deinit(void);

/**
 * @brief Handle kernel events and update UI accordingly
 * @param event Kernel event to handle
 */
void ui_manager_handle_event(const kraken_event_t *event);

/**
 * @brief Periodic update function (called every second)
 * Updates clock, battery status, etc.
 */
void ui_manager_periodic_update(void);

/**
 * @brief Exit current submenu and return to main menu
 */
void ui_manager_exit_submenu(void);

/**
 * @brief Get pointer to current UI status
 * @return Pointer to UI status structure
 */
ui_status_t* ui_manager_get_status(void);

#ifdef __cplusplus
}
#endif
