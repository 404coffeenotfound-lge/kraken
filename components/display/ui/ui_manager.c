#include "ui_internal.h"
#include "kraken/ui_boot_animation.h"
#include "esp_log.h"
#include "esp_sntp.h"
#include <string.h>

static const char *TAG = "ui_manager";

static struct {
    bool initialized;
    ui_status_t status;
    lv_obj_t *screen;
    lv_obj_t *network_screen;
    bool in_submenu;
    bool boot_animation_done;
} g_ui = {0};

// Event handler for kernel events
static void ui_event_handler(const kraken_event_t *event, void *user_data);
static void ui_menu_selection_callback(ui_menu_item_t item);
static void boot_animation_complete(void);

esp_err_t ui_manager_init(lv_obj_t *screen)
{
    if (!screen) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_ui.initialized) {
        return ESP_OK;
    }

    g_ui.screen = screen;
    memset(&g_ui.status, 0, sizeof(g_ui.status));
    g_ui.boot_animation_done = false;

    // Start boot animation first
    ESP_ERROR_CHECK(ui_boot_animation_start(screen, boot_animation_complete));

    g_ui.initialized = true;
    ESP_LOGI(TAG, "UI Manager initialized with boot animation");
    return ESP_OK;
}

static void boot_animation_complete(void)
{
    ESP_LOGI(TAG, "Boot animation complete, initializing main UI");
    
    // Initialize UI components after boot animation
    ESP_ERROR_CHECK(ui_topbar_init(g_ui.screen));
    ESP_ERROR_CHECK(ui_menu_init(g_ui.screen));
    
    // Create network submenu screen (initially hidden)
    g_ui.network_screen = ui_network_screen_create(g_ui.screen);

    // Set menu callback
    ui_menu_set_callback(ui_menu_selection_callback);

    // Subscribe to kernel events
    kraken_event_subscribe(KRAKEN_EVENT_WIFI_CONNECTED, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_WIFI_DISCONNECTED, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_WIFI_SCAN_DONE, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_BT_CONNECTED, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_BT_DISCONNECTED, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_SYSTEM_TIME_SYNC, ui_event_handler, NULL);
    
    // Subscribe to input events for navigation
    kraken_event_subscribe(KRAKEN_EVENT_INPUT_UP, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_INPUT_DOWN, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_INPUT_LEFT, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_INPUT_RIGHT, ui_event_handler, NULL);
    kraken_event_subscribe(KRAKEN_EVENT_INPUT_CENTER, ui_event_handler, NULL);

    g_ui.boot_animation_done = true;
    ESP_LOGI(TAG, "Main UI ready");
}

esp_err_t ui_manager_deinit(void)
{
    if (!g_ui.initialized) {
        return ESP_OK;
    }

    // Unsubscribe from events
    kraken_event_unsubscribe(KRAKEN_EVENT_WIFI_CONNECTED, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_WIFI_DISCONNECTED, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_WIFI_SCAN_DONE, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_BT_CONNECTED, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_BT_DISCONNECTED, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_SYSTEM_TIME_SYNC, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_INPUT_UP, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_INPUT_DOWN, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_INPUT_LEFT, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_INPUT_RIGHT, ui_event_handler);
    kraken_event_unsubscribe(KRAKEN_EVENT_INPUT_CENTER, ui_event_handler);

    g_ui.initialized = false;
    ESP_LOGI(TAG, "UI Manager deinitialized");
    return ESP_OK;
}

void ui_manager_handle_event(const kraken_event_t *event)
{
    if (!g_ui.initialized || !event) {
        return;
    }

    // Ignore input events during boot animation
    if (!g_ui.boot_animation_done) {
        return;
    }

    switch (event->type) {
        case KRAKEN_EVENT_WIFI_SCAN_DONE:
            if (g_ui.in_submenu) {
                ui_network_update_scan_results();
            }
            ESP_LOGI(TAG, "WiFi scan completed");
            break;

        case KRAKEN_EVENT_WIFI_CONNECTED:
            g_ui.status.wifi_connected = true;
            // RSSI would come from event data
            g_ui.status.wifi_rssi = -50;  // Example
            ui_topbar_update_wifi(true, g_ui.status.wifi_rssi);
            
            if (g_ui.in_submenu) {
                ui_network_on_wifi_connected();
            }
            ESP_LOGI(TAG, "WiFi connected");
            break;

        case KRAKEN_EVENT_WIFI_DISCONNECTED:
            g_ui.status.wifi_connected = false;
            ui_topbar_update_wifi(false, 0);
            
            if (g_ui.in_submenu) {
                ui_network_on_wifi_disconnected(false);
            }
            ESP_LOGI(TAG, "WiFi disconnected");
            break;

        case KRAKEN_EVENT_BT_CONNECTED:
            g_ui.status.bt_connected = true;
            ui_topbar_update_bluetooth(g_ui.status.bt_enabled, true);
            ESP_LOGI(TAG, "Bluetooth connected");
            break;

        case KRAKEN_EVENT_BT_DISCONNECTED:
            g_ui.status.bt_connected = false;
            ui_topbar_update_bluetooth(g_ui.status.bt_enabled, false);
            ESP_LOGI(TAG, "Bluetooth disconnected");
            break;

        case KRAKEN_EVENT_SYSTEM_TIME_SYNC:
            // Time synced via NTP
            g_ui.status.time_synced = true;
            time_t now = time(NULL);
            localtime_r(&now, &g_ui.status.current_time);
            ui_topbar_update_time(&g_ui.status.current_time);
            ESP_LOGI(TAG, "Time synchronized");
            break;

        case KRAKEN_EVENT_INPUT_UP:
        case KRAKEN_EVENT_INPUT_DOWN:
        case KRAKEN_EVENT_INPUT_LEFT:
        case KRAKEN_EVENT_INPUT_RIGHT:
            if (g_ui.in_submenu) {
                ui_network_handle_input(event->type);
            } else {
                ui_menu_navigate(event->type);
            }
            break;

        case KRAKEN_EVENT_INPUT_CENTER:
            if (g_ui.in_submenu) {
                // Pass CENTER to submenu for interaction
                ui_network_handle_input(event->type);
            } else {
                ui_menu_select_current();
            }
            break;

        default:
            break;
    }
}

static void ui_event_handler(const kraken_event_t *event, void *user_data)
{
    (void)user_data;
    // Lock LVGL since event handler runs in event task context
    extern void lvgl_port_lock(int timeout_ms);
    extern void lvgl_port_unlock(void);
    
    lvgl_port_lock(0);
    ui_manager_handle_event(event);
    lvgl_port_unlock();
}

static void ui_menu_selection_callback(ui_menu_item_t item)
{
    ESP_LOGI(TAG, "Menu item activated: %d", item);
    
    // Handle menu selection
    switch (item) {
        case UI_MENU_ITEM_AUDIO:
            ESP_LOGI(TAG, "Opening Audio settings");
            // TODO: Open audio settings screen
            break;
            
        case UI_MENU_ITEM_NETWORK:
            ESP_LOGI(TAG, "Opening Network settings");
            // Show network submenu
            g_ui.in_submenu = true;
            ui_network_screen_show();
            break;
            
        case UI_MENU_ITEM_BLUETOOTH:
            ESP_LOGI(TAG, "Opening Bluetooth settings");
            // TODO: Open bluetooth settings screen
            break;
            
        case UI_MENU_ITEM_APPS:
            ESP_LOGI(TAG, "Opening Apps");
            // TODO: Open apps list
            break;
            
        case UI_MENU_ITEM_SETTINGS:
            ESP_LOGI(TAG, "Opening Settings");
            // TODO: Open system settings
            break;
            
        case UI_MENU_ITEM_ABOUT:
            ESP_LOGI(TAG, "Opening About");
            // TODO: Show about dialog
            break;
            
        default:
            break;
    }
}

void ui_manager_exit_submenu(void)
{
    if (g_ui.in_submenu) {
        ESP_LOGI(TAG, "Exiting submenu...");
        g_ui.in_submenu = false;
        ui_network_screen_hide();
        ESP_LOGI(TAG, "Submenu exited, back to main menu");
    }
}

ui_status_t* ui_manager_get_status(void)
{
    return &g_ui.status;
}

// Periodic update task (call this from display service timer)
void ui_manager_periodic_update(void)
{
    if (!g_ui.initialized) {
        return;
    }

    // Update time every second
    if (g_ui.status.time_synced) {
        time_t now = time(NULL);
        localtime_r(&now, &g_ui.status.current_time);
        ui_topbar_update_time(&g_ui.status.current_time);
    }

    // Update battery status (would come from ADC/battery monitor)
    static uint8_t battery_demo = 75;
    g_ui.status.battery_percent = battery_demo;
    g_ui.status.battery_charging = false;
    ui_topbar_update_battery(g_ui.status.battery_percent, g_ui.status.battery_charging);
}
