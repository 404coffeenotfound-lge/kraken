#include "ui_internal.h"
#include "kraken/ui_keyboard.h"
#include "kraken/bt_service.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_bluetooth";

#define TOPBAR_HEIGHT 30
#define DEVICE_LIST_HEIGHT 180
#define DEVICE_ITEM_HEIGHT 40

typedef enum {
    BT_SCREEN_MAIN = 0,
} bt_screen_state_t;

typedef enum {
    FOCUS_BACK_BUTTON = 0,
    FOCUS_BT_TOGGLE,
    FOCUS_DISCONNECT_BUTTON,
    FOCUS_DEVICE_LIST,
} bt_focus_t;

static struct {
    lv_obj_t *screen;
    lv_obj_t *bt_toggle_btn;
    lv_obj_t *bt_toggle_label;
    lv_obj_t *disconnect_button;
    lv_obj_t *device_list;
    lv_obj_t *back_button;
    lv_obj_t *notification;
    
    bt_screen_state_t state;
    bt_focus_t focus;
    bool bt_enabled;
    bool bt_connected;
    uint8_t connected_mac[BT_MAC_ADDR_LEN];
    char connected_name[BT_DEVICE_NAME_MAX_LEN];
    uint8_t selected_mac[BT_MAC_ADDR_LEN];
    bt_scan_result_t scan_results;
    int selected_device_index;
} g_bluetooth = {0};

static void bt_toggle_event_cb(lv_event_t *e);
static void create_device_list(void);
static void update_device_selection(void);
static void connect_to_device(const uint8_t *mac);

static void notification_timer_cb(lv_timer_t *timer)
{
    if (g_bluetooth.notification) {
        lv_obj_del(g_bluetooth.notification);
        g_bluetooth.notification = NULL;
    }
}

void ui_bluetooth_show_notification(const char *message, uint32_t duration_ms)
{
    if (g_bluetooth.notification) {
        lv_obj_del(g_bluetooth.notification);
    }

    g_bluetooth.notification = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_bluetooth.notification, LV_PCT(80), 60);
    lv_obj_align(g_bluetooth.notification, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_bluetooth.notification, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(g_bluetooth.notification, 0, 0);
    lv_obj_set_style_border_color(g_bluetooth.notification, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(g_bluetooth.notification, 2, 0);

    lv_obj_t *label = lv_label_create(g_bluetooth.notification);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
    lv_obj_center(label);

    if (duration_ms > 0) {
        lv_timer_t *timer = lv_timer_create(notification_timer_cb, duration_ms, NULL);
        lv_timer_set_repeat_count(timer, 1);
    }

    ESP_LOGI(TAG, "Notification: %s", message);
}

lv_obj_t *ui_bluetooth_screen_create(lv_obj_t *parent)
{
    g_bluetooth.screen = lv_obj_create(parent);
    lv_obj_set_size(g_bluetooth.screen, LV_HOR_RES, LV_VER_RES - TOPBAR_HEIGHT);
    lv_obj_set_pos(g_bluetooth.screen, 0, TOPBAR_HEIGHT);
    lv_obj_set_style_bg_color(g_bluetooth.screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_bluetooth.screen, 0, 0);
    lv_obj_set_style_radius(g_bluetooth.screen, 0, 0);
    lv_obj_set_style_pad_all(g_bluetooth.screen, 10, 0);
    lv_obj_clear_flag(g_bluetooth.screen, LV_OBJ_FLAG_SCROLLABLE);

    g_bluetooth.back_button = lv_obj_create(g_bluetooth.screen);
    lv_obj_set_size(g_bluetooth.back_button, 40, 30);
    lv_obj_align(g_bluetooth.back_button, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_bluetooth.back_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_bluetooth.back_button, 0, 0);
    lv_obj_set_style_border_width(g_bluetooth.back_button, 1, 0);
    lv_obj_set_style_border_color(g_bluetooth.back_button, lv_color_hex(0x7F7F7F), 0);
    lv_obj_set_style_pad_all(g_bluetooth.back_button, 0, 0);
    lv_obj_clear_flag(g_bluetooth.back_button, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *back_label = lv_label_create(g_bluetooth.back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_set_style_text_color(back_label, lv_color_hex(0x000000), 0);
    lv_obj_center(back_label);

    lv_obj_t *bt_row = lv_obj_create(g_bluetooth.screen);
    lv_obj_set_size(bt_row, LV_HOR_RES - 70, 30);
    lv_obj_align(bt_row, LV_ALIGN_TOP_MID, 25, 0);
    lv_obj_set_style_bg_color(bt_row, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(bt_row, 0, 0);
    lv_obj_set_style_border_width(bt_row, 0, 0);
    lv_obj_set_style_pad_all(bt_row, 5, 0);
    lv_obj_clear_flag(bt_row, LV_OBJ_FLAG_SCROLLABLE);
    
    lv_obj_t *bt_label = lv_label_create(bt_row);
    lv_label_set_text(bt_label, LV_SYMBOL_BLUETOOTH " Bluetooth");
    lv_obj_set_style_text_color(bt_label, lv_color_hex(0x000000), 0);
    lv_obj_align(bt_label, LV_ALIGN_LEFT_MID, 0, 0);
    
    g_bluetooth.bt_toggle_btn = lv_switch_create(bt_row);
    lv_obj_set_size(g_bluetooth.bt_toggle_btn, 40, 20);
    lv_obj_align(g_bluetooth.bt_toggle_btn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(g_bluetooth.bt_toggle_btn, bt_toggle_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    
    g_bluetooth.bt_toggle_label = bt_label;

    g_bluetooth.disconnect_button = lv_obj_create(g_bluetooth.screen);
    lv_obj_set_size(g_bluetooth.disconnect_button, LV_HOR_RES - 20, 35);
    lv_obj_align(g_bluetooth.disconnect_button, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(g_bluetooth.disconnect_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_bluetooth.disconnect_button, 0, 0);
    lv_obj_set_style_border_width(g_bluetooth.disconnect_button, 1, 0);
    lv_obj_set_style_border_color(g_bluetooth.disconnect_button, lv_color_hex(0xFF6B6B), 0);
    lv_obj_set_style_pad_all(g_bluetooth.disconnect_button, 8, 0);
    lv_obj_clear_flag(g_bluetooth.disconnect_button, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_bluetooth.disconnect_button, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *disconnect_label = lv_label_create(g_bluetooth.disconnect_button);
    lv_label_set_text(disconnect_label, LV_SYMBOL_CLOSE " Disconnect");
    lv_obj_set_style_text_color(disconnect_label, lv_color_hex(0xFF0000), 0);
    lv_obj_center(disconnect_label);

    g_bluetooth.device_list = lv_obj_create(g_bluetooth.screen);
    lv_obj_set_size(g_bluetooth.device_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 50);
    lv_obj_align(g_bluetooth.device_list, LV_ALIGN_TOP_MID, 0, 40);
    lv_obj_set_style_bg_color(g_bluetooth.device_list, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_bluetooth.device_list, 0, 0);
    lv_obj_set_style_border_width(g_bluetooth.device_list, 0, 0);
    lv_obj_set_style_pad_all(g_bluetooth.device_list, 0, 0);
    lv_obj_set_flex_flow(g_bluetooth.device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_bluetooth.device_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_scroll_dir(g_bluetooth.device_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_bluetooth.device_list, LV_SCROLLBAR_MODE_OFF);
    
    lv_obj_add_flag(g_bluetooth.screen, LV_OBJ_FLAG_HIDDEN);
    
    g_bluetooth.state = BT_SCREEN_MAIN;
    g_bluetooth.bt_enabled = false;
    g_bluetooth.bt_connected = false;
    g_bluetooth.selected_device_index = 0;

    ESP_LOGI(TAG, "Bluetooth screen created");
    return g_bluetooth.screen;
}

void ui_bluetooth_screen_show(void)
{
    if (!g_bluetooth.screen) {
        return;
    }

    lv_obj_clear_flag(g_bluetooth.screen, LV_OBJ_FLAG_HIDDEN);
    
    g_bluetooth.focus = FOCUS_BACK_BUTTON;
    g_bluetooth.selected_device_index = 0;
    update_device_selection();
    
    g_bluetooth.bt_enabled = bt_service_is_enabled();
    g_bluetooth.bt_connected = bt_service_is_connected();
    
    if (g_bluetooth.bt_enabled) {
        lv_obj_add_state(g_bluetooth.bt_toggle_btn, LV_STATE_CHECKED);
        bt_service_scan(10);
    } else {
        lv_obj_clear_state(g_bluetooth.bt_toggle_btn, LV_STATE_CHECKED);
    }
    
    if (g_bluetooth.disconnect_button && g_bluetooth.device_list) {
        if (g_bluetooth.bt_connected) {
            lv_obj_clear_flag(g_bluetooth.disconnect_button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_t *label = lv_obj_get_child(g_bluetooth.disconnect_button, 0);
            if (label) {
                char text[80];
                snprintf(text, sizeof(text), LV_SYMBOL_CLOSE " Disconnect: %s", 
                        g_bluetooth.connected_name[0] ? g_bluetooth.connected_name : "Unknown");
                lv_label_set_text(label, text);
            }
            lv_obj_set_size(g_bluetooth.device_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 95);
            lv_obj_align(g_bluetooth.device_list, LV_ALIGN_TOP_MID, 0, 85);
        } else {
            lv_obj_add_flag(g_bluetooth.disconnect_button, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_size(g_bluetooth.device_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 50);
            lv_obj_align(g_bluetooth.device_list, LV_ALIGN_TOP_MID, 0, 40);
        }
    }
    
    ESP_LOGI(TAG, "Bluetooth screen shown (BT: %s, Connected: %s)", 
             g_bluetooth.bt_enabled ? "ON" : "OFF",
             g_bluetooth.bt_connected ? "YES" : "NO");
}

void ui_bluetooth_screen_hide(void)
{
    if (g_bluetooth.screen) {
        lv_obj_add_flag(g_bluetooth.screen, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Bluetooth screen hidden");
    }
}

static void bt_toggle_event_cb(lv_event_t *e)
{
    lv_obj_t *sw = lv_event_get_target(e);
    g_bluetooth.bt_enabled = lv_obj_has_state(sw, LV_STATE_CHECKED);
    
    if (g_bluetooth.bt_enabled) {
        bt_service_enable();
        ui_bluetooth_show_notification("Scanning Bluetooth devices...", 2000);
        bt_service_scan(10);
    } else {
        bt_service_disable();
        lv_obj_clean(g_bluetooth.device_list);
    }
    
    ESP_LOGI(TAG, "Bluetooth toggled: %s", g_bluetooth.bt_enabled ? "ON" : "OFF");
}

void ui_bluetooth_update_scan_results(void)
{
    if (!g_bluetooth.bt_enabled) {
        return;
    }

    bt_service_get_scan_results(&g_bluetooth.scan_results);
    
    for (int i = 0; i < g_bluetooth.scan_results.count - 1; i++) {
        for (int j = 0; j < g_bluetooth.scan_results.count - i - 1; j++) {
            if (g_bluetooth.scan_results.devices[j].rssi < g_bluetooth.scan_results.devices[j + 1].rssi) {
                bt_device_info_t temp = g_bluetooth.scan_results.devices[j];
                g_bluetooth.scan_results.devices[j] = g_bluetooth.scan_results.devices[j + 1];
                g_bluetooth.scan_results.devices[j + 1] = temp;
            }
        }
    }
    
    create_device_list();
    
    ESP_LOGI(TAG, "Found %d Bluetooth devices", g_bluetooth.scan_results.count);
}

static void create_device_list(void)
{
    lv_obj_clean(g_bluetooth.device_list);
    
    if (g_bluetooth.scan_results.count == 0) {
        lv_obj_t *label = lv_label_create(g_bluetooth.device_list);
        lv_label_set_text(label, "No devices found");
        lv_obj_set_style_text_color(label, lv_color_hex(0x7F7F7F), 0);
        return;
    }
    
    for (int i = 0; i < g_bluetooth.scan_results.count; i++) {
        bt_device_info_t *dev = &g_bluetooth.scan_results.devices[i];
        
        lv_obj_t *item = lv_obj_create(g_bluetooth.device_list);
        lv_obj_set_width(item, LV_PCT(100));
        lv_obj_set_height(item, 40);
        lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_border_width(item, 0, 0);
        lv_obj_set_style_pad_all(item, 8, 0);
        lv_obj_set_style_pad_left(item, 10, 0);
        lv_obj_set_style_pad_right(item, 10, 0);
        lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
        
        lv_obj_set_style_border_width(item, 1, 0);
        lv_obj_set_style_border_color(item, lv_color_hex(0x7F7F7F), 0);
        lv_obj_set_style_border_side(item, LV_BORDER_SIDE_BOTTOM, 0);
        
        lv_obj_t *icon_label = lv_label_create(item);
        lv_label_set_text(icon_label, LV_SYMBOL_BLUETOOTH);
        lv_obj_set_style_text_color(icon_label, lv_color_hex(0x000000), 0);
        lv_obj_align(icon_label, LV_ALIGN_LEFT_MID, 0, 0);
        
        lv_obj_t *name_label = lv_label_create(item);
        const char *display_name = dev->name[0] ? dev->name : "Unknown Device";
        lv_label_set_text(name_label, display_name);
        lv_label_set_long_mode(name_label, LV_LABEL_LONG_DOT);
        lv_obj_set_width(name_label, 120);
        lv_obj_set_style_text_color(name_label, lv_color_hex(0x000000), 0);
        lv_obj_align(name_label, LV_ALIGN_LEFT_MID, 25, 0);
        
        if (dev->rssi != -1) {
            lv_obj_t *rssi_label = lv_label_create(item);
            char rssi_str[16];
            snprintf(rssi_str, sizeof(rssi_str), "%ddBm", dev->rssi);
            lv_label_set_text(rssi_label, rssi_str);
            lv_obj_set_style_text_color(rssi_label, lv_color_hex(0x7F7F7F), 0);
            lv_obj_align(rssi_label, LV_ALIGN_RIGHT_MID, 0, 0);
        }
        
        lv_obj_set_user_data(item, (void*)(intptr_t)i);
    }
    
    g_bluetooth.selected_device_index = 0;
    update_device_selection();
}

static void update_device_selection(void)
{
    uint32_t child_count = lv_obj_get_child_count(g_bluetooth.device_list);
    
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t *item = lv_obj_get_child(g_bluetooth.device_list, i);
        if (!item) continue;
        
        if (g_bluetooth.focus == FOCUS_DEVICE_LIST && i == g_bluetooth.selected_device_index) {
            lv_obj_set_style_bg_color(item, lv_color_hex(0x808080), 0);
            lv_obj_scroll_to_view(item, LV_ANIM_ON);
        } else {
            lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
        }
    }
    
    if (g_bluetooth.back_button) {
        if (g_bluetooth.focus == FOCUS_BACK_BUTTON) {
            lv_obj_set_style_bg_color(g_bluetooth.back_button, lv_color_hex(0x808080), 0);
        } else {
            lv_obj_set_style_bg_color(g_bluetooth.back_button, lv_color_hex(0xFFFFFF), 0);
        }
    }
    
    if (g_bluetooth.bt_toggle_label) {
        if (g_bluetooth.focus == FOCUS_BT_TOGGLE) {
            lv_obj_set_style_text_color(g_bluetooth.bt_toggle_label, lv_color_hex(0x808080), 0);
        } else {
            lv_obj_set_style_text_color(g_bluetooth.bt_toggle_label, lv_color_hex(0x000000), 0);
        }
    }
    
    if (g_bluetooth.disconnect_button && !lv_obj_has_flag(g_bluetooth.disconnect_button, LV_OBJ_FLAG_HIDDEN)) {
        if (g_bluetooth.focus == FOCUS_DISCONNECT_BUTTON) {
            lv_obj_set_style_bg_color(g_bluetooth.disconnect_button, lv_color_hex(0xFFE0E0), 0);
        } else {
            lv_obj_set_style_bg_color(g_bluetooth.disconnect_button, lv_color_hex(0xFFFFFF), 0);
        }
    }
}

static void connect_to_device(const uint8_t *mac)
{
    esp_err_t ret = bt_service_connect(mac);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Bluetooth connect command sent");
        ui_bluetooth_show_notification("Connecting...", 3000);
    } else {
        ESP_LOGE(TAG, "Failed to initiate Bluetooth connection");
        ui_bluetooth_show_notification("Connection failed!", 5000);
    }
}

void ui_bluetooth_handle_input(kraken_event_type_t input)
{
    if (g_bluetooth.notification) {
        lv_obj_del(g_bluetooth.notification);
        g_bluetooth.notification = NULL;
        return;
    }
    
    if (input == KRAKEN_EVENT_INPUT_UP) {
        if (g_bluetooth.focus == FOCUS_BT_TOGGLE) {
            g_bluetooth.focus = FOCUS_BACK_BUTTON;
            update_device_selection();
            ESP_LOGI(TAG, "Focus: Back button");
        } else if (g_bluetooth.focus == FOCUS_DISCONNECT_BUTTON) {
            g_bluetooth.focus = FOCUS_BT_TOGGLE;
            update_device_selection();
            ESP_LOGI(TAG, "Focus: BT toggle");
        } else if (g_bluetooth.focus == FOCUS_DEVICE_LIST && g_bluetooth.selected_device_index > 0) {
            g_bluetooth.selected_device_index--;
            update_device_selection();
            ESP_LOGI(TAG, "Selected device index: %d", g_bluetooth.selected_device_index);
        } else if (g_bluetooth.focus == FOCUS_DEVICE_LIST && g_bluetooth.selected_device_index == 0) {
            if (g_bluetooth.bt_connected) {
                g_bluetooth.focus = FOCUS_DISCONNECT_BUTTON;
                ESP_LOGI(TAG, "Focus: Disconnect button");
            } else {
                g_bluetooth.focus = FOCUS_BT_TOGGLE;
                ESP_LOGI(TAG, "Focus: BT toggle");
            }
            update_device_selection();
        }
        return;
    }
    
    if (input == KRAKEN_EVENT_INPUT_DOWN) {
        if (g_bluetooth.focus == FOCUS_BACK_BUTTON) {
            g_bluetooth.focus = FOCUS_BT_TOGGLE;
            update_device_selection();
            ESP_LOGI(TAG, "Focus: BT toggle");
        } else if (g_bluetooth.focus == FOCUS_BT_TOGGLE && g_bluetooth.bt_connected) {
            g_bluetooth.focus = FOCUS_DISCONNECT_BUTTON;
            update_device_selection();
            ESP_LOGI(TAG, "Focus: Disconnect button");
        } else if (g_bluetooth.focus == FOCUS_BT_TOGGLE && g_bluetooth.bt_enabled && g_bluetooth.scan_results.count > 0) {
            g_bluetooth.focus = FOCUS_DEVICE_LIST;
            g_bluetooth.selected_device_index = 0;
            update_device_selection();
            ESP_LOGI(TAG, "Focus: Device list, index 0");
        } else if (g_bluetooth.focus == FOCUS_DISCONNECT_BUTTON && g_bluetooth.bt_enabled && g_bluetooth.scan_results.count > 0) {
            g_bluetooth.focus = FOCUS_DEVICE_LIST;
            g_bluetooth.selected_device_index = 0;
            update_device_selection();
            ESP_LOGI(TAG, "Focus: Device list, index 0");
        } else if (g_bluetooth.focus == FOCUS_DEVICE_LIST && 
                   g_bluetooth.selected_device_index < g_bluetooth.scan_results.count - 1) {
            g_bluetooth.selected_device_index++;
            update_device_selection();
            ESP_LOGI(TAG, "Selected device index: %d", g_bluetooth.selected_device_index);
        }
        return;
    }
    
    if (input == KRAKEN_EVENT_INPUT_CENTER) {
        if (g_bluetooth.focus == FOCUS_BT_TOGGLE) {
            if (lv_obj_has_state(g_bluetooth.bt_toggle_btn, LV_STATE_CHECKED)) {
                lv_obj_clear_state(g_bluetooth.bt_toggle_btn, LV_STATE_CHECKED);
                g_bluetooth.bt_enabled = false;
                bt_service_disable();
                lv_obj_clean(g_bluetooth.device_list);
            } else {
                lv_obj_add_state(g_bluetooth.bt_toggle_btn, LV_STATE_CHECKED);
                g_bluetooth.bt_enabled = true;
                bt_service_enable();
                ui_bluetooth_show_notification("Scanning Bluetooth devices...", 2000);
                bt_service_scan(10);
            }
            ESP_LOGI(TAG, "Bluetooth toggled: %s", g_bluetooth.bt_enabled ? "ON" : "OFF");
        } else if (g_bluetooth.focus == FOCUS_DISCONNECT_BUTTON) {
            ESP_LOGI(TAG, "Disconnecting from Bluetooth device");
            bt_service_disconnect();
            ui_bluetooth_show_notification("Disconnecting...", 2000);
        } else if (g_bluetooth.focus == FOCUS_DEVICE_LIST) {
            if (g_bluetooth.selected_device_index < g_bluetooth.scan_results.count) {
                bt_device_info_t *dev = &g_bluetooth.scan_results.devices[g_bluetooth.selected_device_index];
                memcpy(g_bluetooth.selected_mac, dev->mac, BT_MAC_ADDR_LEN);
                
                char mac_str[18];
                snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                        dev->mac[0], dev->mac[1], dev->mac[2], 
                        dev->mac[3], dev->mac[4], dev->mac[5]);
                ESP_LOGI(TAG, "Connecting to: %s (%s)", 
                        dev->name[0] ? dev->name : "Unknown", mac_str);
                
                connect_to_device(dev->mac);
            }
        } else if (g_bluetooth.focus == FOCUS_BACK_BUTTON) {
            ESP_LOGI(TAG, "Back button pressed, exiting submenu");
            extern void ui_manager_exit_submenu(void);
            ui_manager_exit_submenu();
        }
        return;
    }
}

void ui_bluetooth_on_bt_connected(void)
{
    if (g_bluetooth.notification) {
        lv_obj_del(g_bluetooth.notification);
        g_bluetooth.notification = NULL;
    }
    
    g_bluetooth.bt_connected = true;
    memcpy(g_bluetooth.connected_mac, g_bluetooth.selected_mac, BT_MAC_ADDR_LEN);
    
    for (int i = 0; i < g_bluetooth.scan_results.count; i++) {
        if (memcmp(g_bluetooth.scan_results.devices[i].mac, g_bluetooth.selected_mac, BT_MAC_ADDR_LEN) == 0) {
            strncpy(g_bluetooth.connected_name, g_bluetooth.scan_results.devices[i].name, 
                   sizeof(g_bluetooth.connected_name) - 1);
            break;
        }
    }
    
    if (g_bluetooth.disconnect_button) {
        lv_obj_clear_flag(g_bluetooth.disconnect_button, LV_OBJ_FLAG_HIDDEN);
        
        lv_obj_t *label = lv_obj_get_child(g_bluetooth.disconnect_button, 0);
        if (label) {
            char text[80];
            snprintf(text, sizeof(text), LV_SYMBOL_CLOSE " Disconnect: %s", 
                    g_bluetooth.connected_name[0] ? g_bluetooth.connected_name : "Unknown");
            lv_label_set_text(label, text);
        }
        
        if (g_bluetooth.device_list) {
            lv_obj_set_size(g_bluetooth.device_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 95);
            lv_obj_align(g_bluetooth.device_list, LV_ALIGN_TOP_MID, 0, 85);
        }
    }
    
    ui_bluetooth_show_notification("Bluetooth Connected!", 5000);
}

void ui_bluetooth_on_bt_disconnected(bool was_connecting)
{
    if (g_bluetooth.notification) {
        lv_obj_del(g_bluetooth.notification);
        g_bluetooth.notification = NULL;
    }
    
    g_bluetooth.bt_connected = false;
    memset(g_bluetooth.connected_mac, 0, sizeof(g_bluetooth.connected_mac));
    memset(g_bluetooth.connected_name, 0, sizeof(g_bluetooth.connected_name));
    
    if (g_bluetooth.disconnect_button) {
        lv_obj_add_flag(g_bluetooth.disconnect_button, LV_OBJ_FLAG_HIDDEN);
        
        if (g_bluetooth.device_list) {
            lv_obj_set_size(g_bluetooth.device_list, LV_HOR_RES - 20, LV_VER_RES - TOPBAR_HEIGHT - 50);
            lv_obj_align(g_bluetooth.device_list, LV_ALIGN_TOP_MID, 0, 40);
        }
    }
    
    if (g_bluetooth.focus == FOCUS_DISCONNECT_BUTTON) {
        g_bluetooth.focus = FOCUS_BT_TOGGLE;
        update_device_selection();
    }
    
    if (was_connecting) {
        ui_bluetooth_show_notification("Connection Failed!", 5000);
    } else {
        ui_bluetooth_show_notification("Bluetooth Disconnected", 3000);
    }
}
