#include "ui_internal.h"
#include "ui_keyboard.h"
#include "kraken/wifi_service.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_network";

#define TOPBAR_HEIGHT 30
#define NETWORK_LIST_HEIGHT 180
#define NETWORK_ITEM_HEIGHT 40

typedef enum {
    NETWORK_SCREEN_MAIN = 0,      // WiFi toggle + network list
    NETWORK_SCREEN_PASSWORD,      // Password input with keyboard
} network_screen_state_t;

typedef enum {
    FOCUS_BACK_BUTTON = 0,
    FOCUS_WIFI_TOGGLE,
    FOCUS_DISCONNECT_BUTTON,
    FOCUS_NETWORK_LIST,
} network_focus_t;

static struct {
    lv_obj_t *screen;
    lv_obj_t *wifi_toggle_btn;
    lv_obj_t *wifi_toggle_label;
    lv_obj_t *disconnect_button;
    lv_obj_t *network_list;
    lv_obj_t *back_button;
    lv_obj_t *password_screen;
    lv_obj_t *password_input;
    ui_keyboard_t *keyboard;  // Changed to ui_keyboard_t*
    lv_obj_t *notification;
    
    network_screen_state_t state;
    network_focus_t focus;
    bool wifi_enabled;
    bool wifi_connected;
    char connected_ssid[33];
    char selected_ssid[33];
    wifi_scan_result_t scan_results;
    int selected_network_index;
    
    // Keyboard navigation
    uint16_t keyboard_btn_index;
    uint16_t keyboard_btn_count;
} g_network = {0};

static void keyboard_ok_callback(const char *text, void *user_data);
static void keyboard_cancel_callback(const char *text, void *user_data);
static void wifi_toggle_event_cb(lv_event_t *e);
static void network_item_clicked(lv_event_t *e);
static void create_network_list(void);
static void update_network_selection(void);
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

    // Create notification box - black background, white text
    g_network.notification = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_network.notification, LV_PCT(80), 60);
    lv_obj_align(g_network.notification, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_network.notification, lv_color_hex(0xE0E0E0), 0);  // Dark gray
    lv_obj_set_style_radius(g_network.notification, 0, 0);  // Rectangle
    lv_obj_set_style_border_color(g_network.notification, lv_color_hex(0x000000), 0);  // White border
    lv_obj_set_style_border_width(g_network.notification, 2, 0);

    // Message label - white text
    lv_obj_t *label = lv_label_create(g_network.notification);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);  // White text
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
    // Create network settings screen below topbar
    g_network.screen = lv_obj_create(parent);
    lv_obj_set_size(g_network.screen, LV_HOR_RES, LV_VER_RES - TOPBAR_HEIGHT);
    lv_obj_set_pos(g_network.screen, 0, TOPBAR_HEIGHT);  // Below topbar
    lv_obj_set_style_bg_color(g_network.screen, lv_color_hex(0xFFFFFF), 0);  // Black background
    lv_obj_set_style_border_width(g_network.screen, 0, 0);
    lv_obj_set_style_radius(g_network.screen, 0, 0);  // Rectangle
    lv_obj_set_style_pad_all(g_network.screen, 10, 0);
    lv_obj_clear_flag(g_network.screen, LV_OBJ_FLAG_SCROLLABLE);

    // Back button at top - small with just < symbol
    g_network.back_button = lv_obj_create(g_network.screen);
    lv_obj_set_size(g_network.back_button, 40, 30);
    lv_obj_align(g_network.back_button, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_network.back_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_network.back_button, 0, 0);
    lv_obj_set_style_border_width(g_network.back_button, 1, 0);
    lv_obj_set_style_border_color(g_network.back_button, lv_color_hex(0x7F7F7F), 0);
    lv_obj_set_style_pad_all(g_network.back_button, 0, 0);
    lv_obj_clear_flag(g_network.back_button, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_label = lv_label_create(g_network.back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0x000000), 0);
    lv_obj_center(back_label);

    // WiFi Toggle Row - at top, next to back button
    lv_obj_t *wifi_row = lv_obj_create(g_network.screen);
    lv_obj_set_size(wifi_row, LV_HOR_RES - 70, 30);
    lv_obj_align(wifi_row, LV_ALIGN_TOP_MID, 25, 0);
    lv_obj_set_style_bg_color(wifi_row, lv_color_hex(0xFFFFFF), 0);  // Black
    lv_obj_set_style_radius(wifi_row, 0, 0);  // Rectangle
    lv_obj_set_style_border_width(wifi_row, 0, 0);  // No border
    lv_obj_set_style_pad_all(wifi_row, 5, 0);
    lv_obj_clear_flag(wifi_row, LV_OBJ_FLAG_SCROLLABLE);
    
    // WiFi label on left
    lv_obj_t *wifi_label = lv_label_create(wifi_row);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI " WiFi");
    lv_obj_set_style_text_color(wifi_label, lv_color_hex(0x000000), 0);  // White
    lv_obj_align(wifi_label, LV_ALIGN_LEFT_MID, 0, 0);
    
    // Toggle button on right - make it smaller
    g_network.wifi_toggle_btn = lv_switch_create(wifi_row);
    lv_obj_set_size(g_network.wifi_toggle_btn, 40, 20);  // Smaller switch
    lv_obj_align(g_network.wifi_toggle_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(g_network.wifi_toggle_btn, wifi_toggle_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    // Store label for later updates
    g_network.wifi_toggle_label = wifi_label;

    // Disconnect button - initially hidden, shown when connected
    g_network.disconnect_button = lv_obj_create(g_network.screen);
    lv_obj_set_size(g_network.disconnect_button, LV_HOR_RES - 20, 35);
    lv_obj_align(g_network.disconnect_button, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(g_network.disconnect_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_network.disconnect_button, 0, 0);
    lv_obj_set_style_border_width(g_network.disconnect_button, 1, 0);
    lv_obj_set_style_border_color(g_network.disconnect_button, lv_color_hex(0xFF6B6B), 0);  // Red border
    lv_obj_set_style_pad_all(g_network.disconnect_button, 8, 0);
    lv_obj_clear_flag(g_network.disconnect_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_network.disconnect_button, LV_OBJ_FLAG_HIDDEN);  // Hidden by default
    
    lv_obj_t *disconnect_label = lv_label_create(g_network.disconnect_button);
    lv_label_set_text(disconnect_label, LV_SYMBOL_CLOSE " Disconnect WiFi");
    lv_obj_set_style_text_color(disconnect_label, lv_color_hex(0xFF0000), 0);  // Red text
    lv_obj_center(disconnect_label);

    // Network list container - scrollable, position adjusted based on disconnect button visibility
    g_network.network_list = lv_obj_create(g_network.screen);
    lv_obj_set_size(g_network.network_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 50);
    lv_obj_align(g_network.network_list, LV_ALIGN_TOP_MID, 0, 40);  // Below WiFi toggle
    lv_obj_set_style_bg_color(g_network.network_list, lv_color_hex(0xFFFFFF), 0);  // Black
    lv_obj_set_style_radius(g_network.network_list, 0, 0);  // Rectangle
    lv_obj_set_style_border_width(g_network.network_list, 0, 0);  // No border
    lv_obj_set_style_pad_all(g_network.network_list, 0, 0);  // No padding
    lv_obj_set_flex_flow(g_network.network_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_network.network_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(g_network.network_list, LV_DIR_VER);  // Enable vertical scrolling
    lv_obj_set_scrollbar_mode(g_network.network_list, LV_SCROLLBAR_MODE_OFF);  // Hide scrollbar
    
    // Initially hidden
    lv_obj_add_flag(g_network.screen, LV_OBJ_FLAG_HIDDEN);
    
    g_network.state = NETWORK_SCREEN_MAIN;
    g_network.wifi_enabled = false;
    g_network.wifi_connected = false;
    g_network.selected_network_index = 0;

    ESP_LOGI(TAG, "Network screen created (below topbar, scrollable)");
    return g_network.screen;
}

void ui_network_screen_show(void)
{
    if (!g_network.screen) {
        return;
    }

    lv_obj_clear_flag(g_network.screen, LV_OBJ_FLAG_HIDDEN);
    
    // Initialize focus to back button (now at top)
    g_network.focus = FOCUS_BACK_BUTTON;
    g_network.selected_network_index = 0;
    update_network_selection();
    
    // Check WiFi status
    g_network.wifi_enabled = wifi_service_is_enabled();
    g_network.wifi_connected = wifi_service_is_connected();
    
    if (g_network.wifi_enabled) {
        lv_obj_add_state(g_network.wifi_toggle_btn, LV_STATE_CHECKED);
        
        // Start WiFi scan
        wifi_service_scan();
    } else {
        lv_obj_clear_state(g_network.wifi_toggle_btn, LV_STATE_CHECKED);
    }
    
    // Show/hide disconnect button and adjust list based on connection status
    if (g_network.disconnect_button && g_network.network_list) {
        if (g_network.wifi_connected) {
            lv_obj_clear_flag(g_network.disconnect_button, LV_OBJ_FLAG_HIDDEN);
            // TODO: Get actual connected SSID from WiFi service
            lv_obj_t *label = lv_obj_get_child(g_network.disconnect_button, 0);
            if (label) {
                lv_label_set_text(label, LV_SYMBOL_CLOSE " Disconnect WiFi");
            }
            // Adjust network list for disconnect button
            lv_obj_set_size(g_network.network_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 95);
            lv_obj_align(g_network.network_list, LV_ALIGN_TOP_MID, 0, 85);
        } else {
            lv_obj_add_flag(g_network.disconnect_button, LV_OBJ_FLAG_HIDDEN);
            // Restore network list full size
            lv_obj_set_size(g_network.network_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 50);
            lv_obj_align(g_network.network_list, LV_ALIGN_TOP_MID, 0, 40);
        }
    }
    
    ESP_LOGI(TAG, "Network screen shown (WiFi: %s, Connected: %s)", 
             g_network.wifi_enabled ? "ON" : "OFF",
             g_network.wifi_connected ? "YES" : "NO");
}

void ui_network_screen_hide(void)
{
    if (g_network.screen) {
        lv_obj_add_flag(g_network.screen, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Network screen hidden");
    }
    
    // Hide password screen if showing
    hide_password_screen();
}

static void wifi_toggle_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    g_network.wifi_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    if (g_network.wifi_enabled) {
        // Turn ON WiFi
        wifi_service_enable();
        
        // Start scanning
        ui_network_show_notification("Scanning WiFi networks...", 2000);
        wifi_service_scan();
    } else {
        // Turn OFF WiFi
        wifi_service_disable();
        
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
        lv_obj_set_style_text_color(label, lv_color_hex(0x7F7F7F), 0);  // Gray
        return;
    }
    
    // Create list items
    for (int i = 0; i < g_network.scan_results.count; i++) {
        wifi_ap_info_t *net = &g_network.scan_results.aps[i];
        
        // Create container for each network - full width
        lv_obj_t *item = lv_obj_create(g_network.network_list);
        lv_obj_set_width(item, LV_PCT(100));  // Full width
        lv_obj_set_height(item, 40);
        lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);  // Black (normal)
        lv_obj_set_style_radius(item, 0, 0);  // Rectangle
        lv_obj_set_style_border_width(item, 0, 0);  // No border
        lv_obj_set_style_pad_all(item, 8, 0);
        lv_obj_set_style_pad_left(item, 10, 0);  // More padding on left
        lv_obj_set_style_pad_right(item, 10, 0);  // More padding on right
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        
        // Add bottom separator line
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_color(item, lv_color_hex(0x7F7F7F), 0);
        lv_obj_set_style_border_side(item, LV_BORDER_SIDE_BOTTOM, 0);
        
        // Signal strength icon on left
        lv_obj_t *signal_label = lv_label_create(item);
        lv_label_set_text(signal_label, LV_SYMBOL_WIFI);
        lv_obj_set_style_text_color(signal_label, lv_color_hex(0x000000), 0);  // White
        lv_obj_align(signal_label, LV_ALIGN_LEFT_MID, 0, 0);
        
        // SSID label in middle - truncated to fit
        lv_obj_t *ssid_label = lv_label_create(item);
        lv_label_set_text(ssid_label, net->ssid);
        lv_label_set_long_mode(ssid_label, LV_LABEL_LONG_DOT);  // Add ... if too long
        lv_obj_set_width(ssid_label, 120);  // More width for SSID
        lv_obj_set_style_text_color(ssid_label, lv_color_hex(0x000000), 0);  // White
        lv_obj_align(ssid_label, LV_ALIGN_LEFT_MID, 25, 0);
        
        // RSSI value on right
        lv_obj_t *rssi_label = lv_label_create(item);
        char rssi_str[16];
        snprintf(rssi_str, sizeof(rssi_str), "%ddBm", net->rssi);
        lv_label_set_text(rssi_label, rssi_str);
        lv_obj_set_style_text_color(rssi_label, lv_color_hex(0x7F7F7F), 0);  // Gray
        lv_obj_align(rssi_label, LV_ALIGN_RIGHT_MID, 0, 0);
        
        // Store index for selection
        lv_obj_set_user_data(item, (void*)(intptr_t)i);
    }
    
    // Reset selection to first item
    g_network.selected_network_index = 0;
    update_network_selection();
}

static void update_network_selection(void)
{
    uint32_t child_count = lv_obj_get_child_count(g_network.network_list);
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *item = lv_obj_get_child(g_network.network_list, i);
        if (!item) continue;
        
        if (g_network.focus == FOCUS_NETWORK_LIST && i == g_network.selected_network_index) {
            // Selected - gray background
            lv_obj_set_style_bg_color(item, lv_color_hex(0x808080), 0);  // Gray
            // Scroll to make it visible
            lv_obj_scroll_to_view(item, LV_ANIM_ON);
        } else {
            // Normal - black background
            lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);  // Black
        }
    }
    
    // Update back button visual state based on focus
    if (g_network.back_button) {
        if (g_network.focus == FOCUS_BACK_BUTTON) {
            // Focused - gray background
            lv_obj_set_style_bg_color(g_network.back_button, lv_color_hex(0x808080), 0);
        } else {
            // Normal - white background
            lv_obj_set_style_bg_color(g_network.back_button, lv_color_hex(0xFFFFFF), 0);
        }
    }
    
    // Update WiFi toggle visual state based on focus
    if (g_network.wifi_toggle_label) {
        if (g_network.focus == FOCUS_WIFI_TOGGLE) {
            // Focused - change text color or add indicator
            lv_obj_set_style_text_color(g_network.wifi_toggle_label, lv_color_hex(0x808080), 0);  // Gray when focused
        } else {
            lv_obj_set_style_text_color(g_network.wifi_toggle_label, lv_color_hex(0x000000), 0);  // White normally
        }
    }
    
    // Update disconnect button visual state based on focus
    if (g_network.disconnect_button && !lv_obj_has_flag(g_network.disconnect_button, LV_OBJ_FLAG_HIDDEN)) {
        if (g_network.focus == FOCUS_DISCONNECT_BUTTON) {
            // Focused - lighter red background
            lv_obj_set_style_bg_color(g_network.disconnect_button, lv_color_hex(0xFFE0E0), 0);
        } else {
            // Normal - white background
            lv_obj_set_style_bg_color(g_network.disconnect_button, lv_color_hex(0xFFFFFF), 0);
        }
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
    // Create password screen overlay - full screen
    g_network.password_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_network.password_screen, LV_HOR_RES, LV_VER_RES);
    lv_obj_set_pos(g_network.password_screen, 0, 0);
    lv_obj_set_style_bg_color(g_network.password_screen, lv_color_hex(0xFFFFFF), 0);  // Black background
    lv_obj_set_style_radius(g_network.password_screen, 0, 0);
    lv_obj_set_style_border_width(g_network.password_screen, 0, 0);
    lv_obj_clear_flag(g_network.password_screen, LV_OBJ_FLAG_SCROLLABLE);
    
    // Title
    lv_obj_t *title = lv_label_create(g_network.password_screen);
    lv_label_set_text(title, "Connect to:");
    lv_obj_set_style_text_color(title, lv_color_hex(0x000000), 0);  // White text
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 10);
    
    // SSID label
    lv_obj_t *ssid_label = lv_label_create(g_network.password_screen);
    lv_label_set_text(ssid_label, ssid);
    lv_obj_set_style_text_color(ssid_label, lv_color_hex(0x000000), 0);  // White text
    lv_obj_align(ssid_label, LV_ALIGN_TOP_MID, 0, 30);
    
    // Password input (textarea)
    g_network.password_input = lv_textarea_create(g_network.password_screen);
    lv_obj_set_size(g_network.password_input, LV_HOR_RES - 20, 35);
    lv_obj_align(g_network.password_input, LV_ALIGN_TOP_MID, 0, 55);
    lv_textarea_set_placeholder_text(g_network.password_input, "Enter password");
    lv_textarea_set_password_mode(g_network.password_input, true);
    lv_textarea_set_one_line(g_network.password_input, true);
    lv_textarea_set_text(g_network.password_input, "");
    
    // Create custom keyboard
    g_network.keyboard = ui_keyboard_create(g_network.password_screen, g_network.password_input);
    ui_keyboard_set_ok_callback(g_network.keyboard, keyboard_ok_callback, NULL);
    ui_keyboard_set_cancel_callback(g_network.keyboard, keyboard_cancel_callback, NULL);
    
    g_network.state = NETWORK_SCREEN_PASSWORD;
    ESP_LOGI(TAG, "Password screen shown for: %s", ssid);
}

static void hide_password_screen(void)
{
    if (g_network.password_screen) {
        if (g_network.keyboard) {
            ui_keyboard_delete(g_network.keyboard);
            g_network.keyboard = NULL;
        }
        lv_obj_del(g_network.password_screen);
        g_network.password_screen = NULL;
        g_network.password_input = NULL;
        g_network.state = NETWORK_SCREEN_MAIN;
    }
}

static void keyboard_ok_callback(const char *text, void *user_data)
{
    ESP_LOGI(TAG, "Keyboard OK pressed, text length: %d", strlen(text));
    if (strlen(text) > 0) {
        connect_to_wifi(g_network.selected_ssid, text);
        hide_password_screen();
    } else {
        ESP_LOGW(TAG, "Password is empty");
    }
}

static void keyboard_cancel_callback(const char *text, void *user_data)
{
    ESP_LOGI(TAG, "Keyboard cancelled");
    hide_password_screen();
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
        // Password screen - forward input to custom keyboard
        if (g_network.keyboard) {
            ui_keyboard_handle_input(g_network.keyboard, input);
        }
        return;  // In password mode, consume all input
    }
    
    // Main WiFi menu navigation
    if (input == KRAKEN_EVENT_INPUT_UP) {
        if (g_network.focus == FOCUS_WIFI_TOGGLE) {
            // Move from WiFi toggle to back button
            g_network.focus = FOCUS_BACK_BUTTON;
            update_network_selection();
            ESP_LOGI(TAG, "Focus: Back button");
        } else if (g_network.focus == FOCUS_DISCONNECT_BUTTON) {
            // Move from disconnect to WiFi toggle
            g_network.focus = FOCUS_WIFI_TOGGLE;
            update_network_selection();
            ESP_LOGI(TAG, "Focus: WiFi toggle");
        } else if (g_network.focus == FOCUS_NETWORK_LIST && g_network.selected_network_index > 0) {
            // Move up in network list
            g_network.selected_network_index--;
            update_network_selection();
            ESP_LOGI(TAG, "Selected network index: %d", g_network.selected_network_index);
        } else if (g_network.focus == FOCUS_NETWORK_LIST && g_network.selected_network_index == 0) {
            // Move focus to disconnect button or WiFi toggle
            if (g_network.wifi_connected) {
                g_network.focus = FOCUS_DISCONNECT_BUTTON;
                ESP_LOGI(TAG, "Focus: Disconnect button");
            } else {
                g_network.focus = FOCUS_WIFI_TOGGLE;
                ESP_LOGI(TAG, "Focus: WiFi toggle");
            }
            update_network_selection();
        }
        return;
    }
    
    if (input == KRAKEN_EVENT_INPUT_DOWN) {
        if (g_network.focus == FOCUS_BACK_BUTTON) {
            // Move from back button to WiFi toggle
            g_network.focus = FOCUS_WIFI_TOGGLE;
            update_network_selection();
            ESP_LOGI(TAG, "Focus: WiFi toggle");
        } else if (g_network.focus == FOCUS_WIFI_TOGGLE && g_network.wifi_connected) {
            // Move to disconnect button when connected
            g_network.focus = FOCUS_DISCONNECT_BUTTON;
            update_network_selection();
            ESP_LOGI(TAG, "Focus: Disconnect button");
        } else if (g_network.focus == FOCUS_WIFI_TOGGLE && g_network.wifi_enabled && g_network.scan_results.count > 0) {
            // Move focus to network list
            g_network.focus = FOCUS_NETWORK_LIST;
            g_network.selected_network_index = 0;
            update_network_selection();
            ESP_LOGI(TAG, "Focus: Network list, index 0");
        } else if (g_network.focus == FOCUS_DISCONNECT_BUTTON && g_network.wifi_enabled && g_network.scan_results.count > 0) {
            // Move from disconnect to network list
            g_network.focus = FOCUS_NETWORK_LIST;
            g_network.selected_network_index = 0;
            update_network_selection();
            ESP_LOGI(TAG, "Focus: Network list, index 0");
        } else if (g_network.focus == FOCUS_NETWORK_LIST && 
                   g_network.selected_network_index < g_network.scan_results.count - 1) {
            // Move down in network list
            g_network.selected_network_index++;
            update_network_selection();
            ESP_LOGI(TAG, "Selected network index: %d", g_network.selected_network_index);
        }
        return;
    }
    
    if (input == KRAKEN_EVENT_INPUT_CENTER) {
        if (g_network.focus == FOCUS_WIFI_TOGGLE) {
            // Toggle WiFi ON/OFF
            if (lv_obj_has_state(g_network.wifi_toggle_btn, LV_STATE_CHECKED)) {
                lv_obj_clear_state(g_network.wifi_toggle_btn, LV_STATE_CHECKED);
                g_network.wifi_enabled = false;
                wifi_service_disable();
                lv_obj_clean(g_network.network_list);
            } else {
                lv_obj_add_state(g_network.wifi_toggle_btn, LV_STATE_CHECKED);
                g_network.wifi_enabled = true;
                wifi_service_enable();
                ui_network_show_notification("Scanning WiFi networks...", 2000);
                wifi_service_scan();
            }
            ESP_LOGI(TAG, "WiFi toggled: %s", g_network.wifi_enabled ? "ON" : "OFF");
        } else if (g_network.focus == FOCUS_DISCONNECT_BUTTON) {
            // Disconnect from WiFi
            ESP_LOGI(TAG, "Disconnecting from WiFi: %s", g_network.connected_ssid);
            wifi_service_disconnect();
            ui_network_show_notification("Disconnecting...", 2000);
        } else if (g_network.focus == FOCUS_NETWORK_LIST) {
            // Connect to selected network
            if (g_network.selected_network_index < g_network.scan_results.count) {
                strncpy(g_network.selected_ssid, 
                        g_network.scan_results.aps[g_network.selected_network_index].ssid,
                        sizeof(g_network.selected_ssid) - 1);
                ESP_LOGI(TAG, "Connecting to: %s", g_network.selected_ssid);
                show_password_screen(g_network.selected_ssid);
            }
        } else if (g_network.focus == FOCUS_BACK_BUTTON) {
            // Back button pressed - exit to main menu
            ESP_LOGI(TAG, "Back button pressed, exiting submenu");
            extern void ui_manager_exit_submenu(void);
            ui_manager_exit_submenu();
        }
        return;
    }
}

void ui_network_on_wifi_connected(void)
{
    // Dismiss connecting notification
    if (g_network.notification) {
        lv_obj_del(g_network.notification);
        g_network.notification = NULL;
    }
    
    // Mark as connected and store SSID
    g_network.wifi_connected = true;
    strncpy(g_network.connected_ssid, g_network.selected_ssid, sizeof(g_network.connected_ssid) - 1);
    
    // Show disconnect button and adjust network list
    if (g_network.disconnect_button) {
        lv_obj_clear_flag(g_network.disconnect_button, LV_OBJ_FLAG_HIDDEN);
        
        // Update disconnect button text with SSID
        lv_obj_t *label = lv_obj_get_child(g_network.disconnect_button, 0);
        if (label) {
            char text[64];
            snprintf(text, sizeof(text), LV_SYMBOL_CLOSE " Disconnect: %s", g_network.connected_ssid);
            lv_label_set_text(label, text);
        }
        
        // Adjust network list position and size
        if (g_network.network_list) {
            lv_obj_set_size(g_network.network_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 95);
            lv_obj_align(g_network.network_list, LV_ALIGN_TOP_MID, 0, 85);
        }
    }
    
    // Show success notification
    ui_network_show_notification("WiFi Connected!", 5000);
}

void ui_network_on_wifi_disconnected(bool was_connecting)
{
    // Dismiss connecting notification
    if (g_network.notification) {
        lv_obj_del(g_network.notification);
        g_network.notification = NULL;
    }
    
    // Mark as disconnected
    g_network.wifi_connected = false;
    memset(g_network.connected_ssid, 0, sizeof(g_network.connected_ssid));
    
    // Hide disconnect button and restore network list size
    if (g_network.disconnect_button) {
        lv_obj_add_flag(g_network.disconnect_button, LV_OBJ_FLAG_HIDDEN);
        
        // Restore network list position and size
        if (g_network.network_list) {
            lv_obj_set_size(g_network.network_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 50);
            lv_obj_align(g_network.network_list, LV_ALIGN_TOP_MID, 0, 40);
        }
    }
    
    // Adjust focus if it was on disconnect button
    if (g_network.focus == FOCUS_DISCONNECT_BUTTON) {
        g_network.focus = FOCUS_WIFI_TOGGLE;
        update_network_selection();
    }
    
    if (was_connecting) {
        // Connection failed
        ui_network_show_notification("Connection Failed!", 5000);
    } else {
        // Normal disconnect
        ui_network_show_notification("WiFi Disconnected", 3000);
    }
}
