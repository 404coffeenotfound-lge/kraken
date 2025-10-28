#pragma once

#include "lvgl.h"
#include "kraken/kernel.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Menu item identifiers
 */
typedef enum {
    UI_MENU_ITEM_AUDIO = 0,
    UI_MENU_ITEM_NETWORK,
    UI_MENU_ITEM_BLUETOOTH,
    UI_MENU_ITEM_APPS,
    UI_MENU_ITEM_SETTINGS,
    UI_MENU_ITEM_ABOUT,
    UI_MENU_ITEM_COUNT
} ui_menu_item_t;

/**
 * @brief Menu selection callback function type
 * @param item The menu item that was selected
 */
typedef void (*ui_menu_callback_t)(ui_menu_item_t item);

/**
 * @brief Initialize the main menu
 * @param parent Parent LVGL object to attach the menu to
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t ui_menu_init(lv_obj_t *parent);

/**
 * @brief Set callback for menu item selection
 * @param callback Function to call when a menu item is selected
 */
void ui_menu_set_callback(ui_menu_callback_t callback);

/**
 * @brief Navigate through menu items
 * @param direction Navigation direction (INPUT_UP, INPUT_DOWN, etc.)
 */
void ui_menu_navigate(kraken_event_type_t direction);

/**
 * @brief Trigger selection of current menu item
 */
void ui_menu_select_current(void);

/**
 * @brief Get currently selected menu item
 * @return Current menu item
 */
ui_menu_item_t ui_menu_get_selected(void);

#ifdef __cplusplus
}
#endif
