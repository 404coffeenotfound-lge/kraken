#include "ui_internal.h"
#include "kraken/wifi_service.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_network";

#define NETWORK_LIST_HEIGHT 180
#define NETWORK_ITEM_HEIGHT 40

typedef enum {
    NETWORK_SCREEN_MAIN = 0,      // WiFi toggle + network list
    NETWORK_SCREEN_PASSWORD,      // Password input with keyboard
} network_screen_state_t;

static struct {
    lv_obj_t *screen;
    lv_obj_t *wifi_toggle_btn;
    lv_obj_t *wifi_toggle_label;
    lv_obj_t *network_list;
    lv_obj_t *password_screen;
    lv_obj_t *password_input;
    lv_obj_t *keyboard;
    lv_obj_t *notification;
    
    network_screen_state_t state;
    bool wifi_enabled;
    char selected_ssid[33];
    wifi_scan_result_t scan_results;
    int selected_network_index;
} g_network = {0};

static void wifi_toggle_event_cb(lv_event_t *e);
static void network_item_clicked(lv_event_t *e);
static void keyboard_event_cb(lv_event_t *e);
static void create_network_list(void);
static void show_password_screen(const char *ssid);
static void hide_password_screen(void);
static void connect_to_wifi(const char *ssid, const char *password);

// Notification timer callback
static void notification_timer_cb(lv_timer_t *timer)
{
    if (g_network.notification) {
        lv_obj_del(g_network.notification);
        g_network.notification = NULL;
    }
}

void ui_network_show_notification(const char *message, uint32_t duration_ms)
{
    // Remove existing notification if any
    if (g_network.notification) {
        lv_obj_del(g_network.notification);
    }

    // Create notification box
    g_network.notification = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_network.notification, LV_PCT(80), 60);
    lv_obj_align(g_network.notification, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_network.notification, lv_color_hex(0x333333), 0);
    lv_obj_set_style_border_color(g_network.notification, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(g_network.notification, 2, 0);
    lv_obj_set_style_radius(g_network.notification, 10, 0);

    // Message label
    lv_obj_t *label = lv_label_create(g_network.notification);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(label);

    // Auto-dismiss after duration
    if (duration_ms > 0) {
        lv_timer_t *timer = lv_timer_create(notification_timer_cb, duration_ms, NULL);
        lv_timer_set_repeat_count(timer, 1);
    }

    ESP_LOGI(TAG, "Notification: %s", message);
}

lv_obj_t *ui_network_screen_create(lv_obj_t *parent)
{
    // Create network settings screen
    g_network.screen = lv_obj_create(parent);
    lv_obj_set_size(g_network.screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_network.screen, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(g_network.screen, 0, 0);
    lv_obj_set_flex_flow(g_network.screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_network.screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_pad_all(g_network.screen, 10, 0);

    // Title
    lv_obj_t *title = lv_label_create(g_network.screen);
    lv_label_set_text(title, "Network Settings");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);

    // WiFi Toggle Button
    g_network.wifi_toggle_btn = lv_btn_create(g_network.screen);
    lv_obj_set_size(g_network.wifi_toggle_btn, LV_PCT(90), 40);
    lv_obj_add_event_cb(g_network.wifi_toggle_btn, wifi_toggle_event_cb, LV_EVENT_CLICKED, NULL);
    
    g_network.wifi_toggle_label = lv_label_create(g_network.wifi_toggle_btn);
    lv_label_set_text(g_network.wifi_toggle_label, LV_SYMBOL_WIFI " WiFi: OFF");
    lv_obj_center(g_network.wifi_toggle_label);

    // Network list container
    g_network.network_list = lv_obj_create(g_network.screen);
    lv_obj_set_size(g_network.network_list, LV_PCT(90), NETWORK_LIST_HEIGHT);
    lv_obj_set_style_bg_color(g_network.network_list, lv_color_hex(0x111111), 0);
    lv_obj_set_flex_flow(g_network.network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_network.network_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    
    // Initially hidden
    lv_obj_add_flag(g_network.screen, LV_OBJ_FLAG_HIDDEN);
    
    g_network.state = NETWORK_SCREEN_MAIN;
    g_network.wifi_enabled = false;
    g_network.selected_network_index = 0;

    ESP_LOGI(TAG, "Network screen created");
    return g_network.screen;
}

void ui_network_screen_show(void)
{
    if (!g_network.screen) {
        return;
    }

    lv_obj_clear_flag(g_network.screen, LV_OBJ_FLAG_HIDDEN);
    
    // Check WiFi status
    g_network.wifi_enabled = wifi_service_is_enabled();
    
    if (g_network.wifi_enabled) {
        lv_label_set_text(g_network.wifi_toggle_label, LV_SYMBOL_WIFI " WiFi: ON");
        lv_obj_set_style_bg_color(g_network.wifi_toggle_btn, lv_color_hex(0x00AA00), 0);
        
        // Start WiFi scan
        wifi_service_scan();
    } else {
        lv_label_set_text(g_network.wifi_toggle_label, LV_SYMBOL_WIFI " WiFi: OFF");
        lv_obj_set_style_bg_color(g_network.wifi_toggle_btn, lv_color_hex(0x555555), 0);
    }
    
    ESP_LOGI(TAG, "Network screen shown");
}

void ui_network_screen_hide(void)
{
    if (g_network.screen) {
        lv_obj_add_flag(g_network.screen, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Hide password screen if showing
    hide_password_screen();
}

static void wifi_toggle_event_cb(lv_event_t *e)
{
    g_network.wifi_enabled = !g_network.wifi_enabled;
    
    if (g_network.wifi_enabled) {
        // Turn ON WiFi
        wifi_service_enable();
        lv_label_set_text(g_network.wifi_toggle_label, LV_SYMBOL_WIFI " WiFi: ON");
        lv_obj_set_style_bg_color(g_network.wifi_toggle_btn, lv_color_hex(0x00AA00), 0);
        
        // Start scanning
        ui_network_show_notification("Scanning WiFi networks...", 2000);
        wifi_service_scan();
    } else {
        // Turn OFF WiFi
        wifi_service_disable();
        lv_label_set_text(g_network.wifi_toggle_label, LV_SYMBOL_WIFI " WiFi: OFF");
        lv_obj_set_style_bg_color(g_network.wifi_toggle_btn, lv_color_hex(0x555555), 0);
        
        // Clear network list
        lv_obj_clean(g_network.network_list);
    }
    
    ESP_LOGI(TAG, "WiFi toggled: %s", g_network.wifi_enabled ? "ON" : "OFF");
}

void ui_network_update_scan_results(void)
{
    if (!g_network.wifi_enabled) {
        return;
    }

    // Get scan results
    wifi_service_get_scan_results(&g_network.scan_results);
    
    // Sort by signal strength (RSSI) - bubble sort for simplicity
    for (int i = 0; i < g_network.scan_results.count - 1; i++) {
        for (int j = 0; j < g_network.scan_results.count - i - 1; j++) {
            if (g_network.scan_results.aps[j].rssi < g_network.scan_results.aps[j + 1].rssi) {
                // Swap
                wifi_ap_info_t temp = g_network.scan_results.aps[j];
                g_network.scan_results.aps[j] = g_network.scan_results.aps[j + 1];
                g_network.scan_results.aps[j + 1] = temp;
            }
        }
    }
    
    // Create network list UI
    create_network_list();
    
    ESP_LOGI(TAG, "Found %d networks", g_network.scan_results.count);
}

static void create_network_list(void)
{
    // Clear existing list
    lv_obj_clean(g_network.network_list);
    
    if (g_network.scan_results.count == 0) {
        lv_obj_t *label = lv_label_create(g_network.network_list);
        lv_label_set_text(label, "No networks found");
        lv_obj_set_style_text_color(label, lv_color_hex(0x888888), 0);
        return;
    }
    
    // Create list items
    for (int i = 0; i < g_network.scan_results.count; i++) {
        wifi_ap_info_t *net = &g_network.scan_results.aps[i];
        
        // Create button for each network
        lv_obj_t *btn = lv_btn_create(g_network.network_list);
        lv_obj_set_size(btn, LV_PCT(95), NETWORK_ITEM_HEIGHT);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x222222), 0);
        lv_obj_add_event_cb(btn, network_item_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
        
        // Container for SSID and signal strength
        lv_obj_t *container = lv_obj_create(btn);
        lv_obj_set_size(container, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(container, 0, 0);
        lv_obj_set_style_pad_all(container, 0, 0);
        
        // SSID label
        lv_obj_t *ssid_label = lv_label_create(container);
        lv_label_set_text(ssid_label, net->ssid);
        lv_obj_set_style_text_color(ssid_label, lv_color_hex(0xFFFFFF), 0);
        lv_obj_align(ssid_label, LV_ALIGN_LEFT_MID, 5, 0);
        
        // Signal strength indicator
        lv_obj_t *rssi_label = lv_label_create(container);
        char rssi_str[32];
        const char *signal_icon;
        lv_color_t signal_color;
        
        if (net->rssi > -50) {
            signal_icon = LV_SYMBOL_WIFI " Strong";
            signal_color = lv_color_hex(0x00FF00);
        } else if (net->rssi > -70) {
            signal_icon = LV_SYMBOL_WIFI " Medium";
            signal_color = lv_color_hex(0xFFFF00);
        } else {
            signal_icon = LV_SYMBOL_WIFI " Weak";
            signal_color = lv_color_hex(0xFF8800);
        }
        
        snprintf(rssi_str, sizeof(rssi_str), "%s (%ddBm)", signal_icon, net->rssi);
        lv_label_set_text(rssi_label, rssi_str);
        lv_obj_set_style_text_color(rssi_label, signal_color, 0);
        lv_obj_align(rssi_label, LV_ALIGN_RIGHT_MID, -5, 0);
    }
}

static void network_item_clicked(lv_event_t *e)
{
    int index = (int)(intptr_t)lv_event_get_user_data(e);
    
    if (index >= 0 && index < g_network.scan_results.count) {
        g_network.selected_network_index = index;
        strncpy(g_network.selected_ssid, g_network.scan_results.aps[index].ssid, sizeof(g_network.selected_ssid) - 1);
        
        ESP_LOGI(TAG, "Selected network: %s", g_network.selected_ssid);
        
        // Show password input screen
        show_password_screen(g_network.selected_ssid);
    }
}

static void show_password_screen(const char *ssid)
{
    // Create password screen overlay
    g_network.password_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_network.password_screen, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(g_network.password_screen, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(g_network.password_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t *title = lv_label_create(g_network.password_screen);
    char title_text[64];
    snprintf(title_text, sizeof(title_text), "Connect to: %s", ssid);
    lv_label_set_text(title, title_text);
    lv_obj_set_style_text_color(title, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // Password input (textarea)
    g_network.password_input = lv_textarea_create(g_network.password_screen);
    lv_obj_set_size(g_network.password_input, LV_PCT(90), 40);
    lv_obj_align(g_network.password_input, LV_ALIGN_TOP_MID, 0, 40);
    lv_textarea_set_placeholder_text(g_network.password_input, "Enter password");
    lv_textarea_set_password_mode(g_network.password_input, true);
    lv_textarea_set_one_line(g_network.password_input, true);
    lv_textarea_set_text(g_network.password_input, "");
    
    // Virtual keyboard
    g_network.keyboard = lv_keyboard_create(g_network.password_screen);
    lv_obj_set_size(g_network.keyboard, LV_PCT(100), LV_PCT(50));
    lv_obj_align(g_network.keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_keyboard_set_textarea(g_network.keyboard, g_network.password_input);
    lv_keyboard_set_mode(g_network.keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
    
    // Handle keyboard events (OK/Cancel)
    lv_obj_add_event_cb(g_network.keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(g_network.keyboard, keyboard_event_cb, LV_EVENT_CANCEL, NULL);
    
    g_network.state = NETWORK_SCREEN_PASSWORD;
    ESP_LOGI(TAG, "Password screen shown for: %s", ssid);
}

static void hide_password_screen(void)
{
    if (g_network.password_screen) {
        lv_obj_del(g_network.password_screen);
        g_network.password_screen = NULL;
        g_network.password_input = NULL;
        g_network.keyboard = NULL;
        g_network.state = NETWORK_SCREEN_MAIN;
    }
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    
    if (code == LV_EVENT_READY) {
        // User pressed OK on keyboard
        const char *password = lv_textarea_get_text(g_network.password_input);
        
        ESP_LOGI(TAG, "Connecting to %s...", g_network.selected_ssid);
        
        // Hide password screen
        hide_password_screen();
        
        // Show connecting notification
        ui_network_show_notification("Connecting...", 0);
        
        // Connect to WiFi
        connect_to_wifi(g_network.selected_ssid, password);
        
    } else if (code == LV_EVENT_CANCEL) {
        // User pressed Cancel
        ESP_LOGI(TAG, "Password input cancelled");
        hide_password_screen();
    }
}

static void connect_to_wifi(const char *ssid, const char *password)
{
    esp_err_t ret = wifi_service_connect(ssid, password);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi connect command sent");
        // Actual connection result will come via event
    } else {
        ESP_LOGE(TAG, "Failed to initiate WiFi connection");
        ui_network_show_notification("Connection failed!", 5000);
    }
}

void ui_network_handle_input(kraken_event_type_t input)
{
    // Dismiss notification on any button press
    if (g_network.notification) {
        lv_obj_del(g_network.notification);
        g_network.notification = NULL;
        return;  // Button consumed for dismissing notification
    }
    
    // Handle input events for navigation
    if (g_network.state == NETWORK_SCREEN_PASSWORD) {
        // Password screen handles its own input via LVGL keyboard
        return;
    }
    
    // TODO: Add navigation support for network list if needed
    // For now, user can use RIGHT button on main menu to enter network settings
}

void ui_network_on_wifi_connected(void)
{
    // Dismiss connecting notification
    if (g_network.notification) {
        lv_obj_del(g_network.notification);
        g_network.notification = NULL;
    }
    
    // Show success notification
    ui_network_show_notification("WiFi Connected!", 5000);
    
    // Go back to main menu after delay
    // TODO: Could add a timer to auto-close the network screen
}

void ui_network_on_wifi_disconnected(bool was_connecting)
{
    // Dismiss connecting notification
    if (g_network.notification) {
        lv_obj_del(g_network.notification);
        g_network.notification = NULL;
    }
    
    if (was_connecting) {
        // Connection failed
        ui_network_show_notification("Connection Failed!", 5000);
    } else {
        // Normal disconnect
        ui_network_show_notification("WiFi Disconnected", 3000);
    }
}
