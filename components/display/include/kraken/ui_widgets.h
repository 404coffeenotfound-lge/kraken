#pragma once

#include "lvgl.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create an icon with label widget
 * @param parent Parent LVGL object
 * @param symbol LVGL symbol (e.g., LV_SYMBOL_WIFI)
 * @param text Label text
 * @return Created widget object
 */
lv_obj_t *ui_create_icon_label(lv_obj_t *parent, const char *symbol, const char *text);

/**
 * @brief Create a menu item widget
 * @param parent Parent LVGL object
 * @param title Item title text
 * @param icon Icon symbol (LVGL symbol)
 * @return Created menu item object
 */
lv_obj_t *ui_create_menu_item(lv_obj_t *parent, const char *title, const char *icon);

/**
 * @brief Set visual selection state of a menu item
 * @param item Menu item object
 * @param selected True to highlight, false for normal state
 */
void ui_set_menu_item_selected(lv_obj_t *item, bool selected);

#ifdef __cplusplus
}
#endif
