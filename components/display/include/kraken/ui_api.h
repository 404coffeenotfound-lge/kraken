#pragma once

/**
 * @file ui_api.h
 * @brief Kraken OS UI Framework - Public API
 * 
 * This is the main header for user applications to build custom UIs.
 * Include this file in your app to access all UI components.
 * 
 * @example Simple App UI
 * ```c
 * #include "kraken/ui_api.h"
 * 
 * void my_app_create_ui(lv_obj_t *parent) {
 *     // Create a custom screen
 *     lv_obj_t *screen = lv_obj_create(parent);
 *     
 *     // Use Kraken UI widgets
 *     lv_obj_t *menu_item = ui_create_menu_item(screen, "My Feature", LV_SYMBOL_HOME);
 *     
 *     // Show notifications
 *     ui_network_show_notification("App Started!", 3000);
 * }
 * ```
 */

// Core UI components
#include "kraken/ui_manager.h"       // UI lifecycle management
#include "kraken/ui_topbar.h"        // Status bar APIs
#include "kraken/ui_menu.h"          // Main menu APIs
#include "kraken/ui_widgets.h"       // Reusable widgets
#include "kraken/ui_network.h"       // Network settings UI
#include "kraken/ui_boot_animation.h" // Boot animation

// LVGL - the underlying graphics library
#include "lvgl.h"

// Kernel APIs for events and services
#include "kraken/kernel.h"

/**
 * @defgroup UI_API Kraken UI Framework
 * @{
 */

/**
 * @defgroup UI_Manager UI Manager
 * @brief High-level UI lifecycle and event management
 * @{
 * @}
 */

/**
 * @defgroup UI_TopBar Status Bar
 * @brief Top status bar with clock and icons
 * @{
 * @}
 */

/**
 * @defgroup UI_Menu Main Menu
 * @brief Main application menu
 * @{
 * @}
 */

/**
 * @defgroup UI_Widgets Reusable Widgets
 * @brief Common UI components
 * @{
 * @}
 */

/**
 * @defgroup UI_Network Network Settings
 * @brief WiFi configuration screen
 * @{
 * @}
 */

/**
 * @}
 */

// Quick start guide for app developers
#if 0
/**
 * @page ui_quick_start Quick Start Guide
 * 
 * ## Creating a Custom App UI
 * 
 * ### 1. Basic Setup
 * ```c
 * #include "kraken/ui_api.h"
 * 
 * static lv_obj_t *my_screen = NULL;
 * 
 * void my_app_init(void) {
 *     // Create your screen
 *     my_screen = lv_obj_create(lv_screen_active());
 *     lv_obj_set_size(my_screen, LV_PCT(100), LV_PCT(100));
 *     
 *     // Add content
 *     lv_obj_t *label = lv_label_create(my_screen);
 *     lv_label_set_text(label, "Hello Kraken!");
 *     lv_obj_center(label);
 * }
 * ```
 * 
 * ### 2. Using Kraken Widgets
 * ```c
 * // Create menu-style items
 * lv_obj_t *item = ui_create_menu_item(my_screen, "Settings", LV_SYMBOL_SETTINGS);
 * 
 * // Create icon+label combos
 * lv_obj_t *status = ui_create_icon_label(my_screen, LV_SYMBOL_WIFI, "Connected");
 * ```
 * 
 * ### 3. Handling Events
 * ```c
 * static void my_event_handler(const kraken_event_t *evt, void *user_data) {
 *     if (evt->type == KRAKEN_EVENT_INPUT_CENTER) {
 *         ui_network_show_notification("Button pressed!", 2000);
 *     }
 * }
 * 
 * void my_app_start(void) {
 *     kraken_event_subscribe(KRAKEN_EVENT_INPUT_CENTER, my_event_handler, NULL);
 * }
 * ```
 * 
 * ### 4. Show/Hide Screens
 * ```c
 * void my_app_show(void) {
 *     lv_obj_clear_flag(my_screen, LV_OBJ_FLAG_HIDDEN);
 * }
 * 
 * void my_app_hide(void) {
 *     lv_obj_add_flag(my_screen, LV_OBJ_FLAG_HIDDEN);
 * }
 * ```
 * 
 * ### 5. Notifications
 * ```c
 * // Show a message
 * ui_network_show_notification("Task completed!", 5000);  // 5 second auto-dismiss
 * 
 * // Persistent notification (duration_ms = 0)
 * ui_network_show_notification("Please wait...", 0);
 * ```
 * 
 * ## Available LVGL Widgets
 * 
 * All standard LVGL widgets are available:
 * - `lv_label_create()` - Text labels
 * - `lv_btn_create()` - Buttons
 * - `lv_list_create()` - Lists
 * - `lv_dropdown_create()` - Dropdowns
 * - `lv_textarea_create()` - Text input
 * - `lv_keyboard_create()` - Virtual keyboard
 * - `lv_chart_create()` - Charts
 * - And many more...
 * 
 * See LVGL documentation: https://docs.lvgl.io/
 */
#endif
