#include "ui_internal.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "ui_topbar";

#define TOPBAR_HEIGHT 30
#define ICON_SIZE 20

static struct {
    lv_obj_t *bar;
    lv_obj_t *time_label;
    lv_obj_t *wifi_icon;
    lv_obj_t *bt_icon;
    lv_obj_t *battery_icon;
    lv_obj_t *battery_label;
} g_topbar = {0};

// LVGL built-in symbols
#define WIFI_SYMBOL LV_SYMBOL_WIFI
#define BT_SYMBOL LV_SYMBOL_BLUETOOTH
#define BATTERY_SYMBOL LV_SYMBOL_BATTERY_FULL

esp_err_t ui_topbar_init(lv_obj_t *parent)
{
    if (!parent) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create top bar container
    g_topbar.bar = lv_obj_create(parent);
    lv_obj_set_size(g_topbar.bar, LV_PCT(100), TOPBAR_HEIGHT);
    lv_obj_align(g_topbar.bar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(g_topbar.bar, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_width(g_topbar.bar, 0, 0);
    lv_obj_set_style_pad_all(g_topbar.bar, 5, 0);
    lv_obj_clear_flag(g_topbar.bar, LV_OBJ_FLAG_SCROLLABLE);

    // Digital clock in center
    g_topbar.time_label = lv_label_create(g_topbar.bar);
    lv_label_set_text(g_topbar.time_label, "--:--");
    lv_obj_set_style_text_color(g_topbar.time_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(g_topbar.time_label, LV_ALIGN_CENTER, 0, 0);

    // Battery icon + percent (rightmost)
    g_topbar.battery_icon = lv_label_create(g_topbar.bar);
    lv_label_set_text(g_topbar.battery_icon, BATTERY_SYMBOL);
    lv_obj_set_style_text_color(g_topbar.battery_icon, lv_color_hex(0x00FF00), 0);
    lv_obj_align(g_topbar.battery_icon, LV_ALIGN_RIGHT_MID, -25, 0);

    g_topbar.battery_label = lv_label_create(g_topbar.bar);
    lv_label_set_text(g_topbar.battery_label, "??%");
    lv_obj_set_style_text_color(g_topbar.battery_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(g_topbar.battery_label, LV_ALIGN_RIGHT_MID, 0, 0);

    // Bluetooth icon
    g_topbar.bt_icon = lv_label_create(g_topbar.bar);
    lv_label_set_text(g_topbar.bt_icon, BT_SYMBOL);
    lv_obj_set_style_text_color(g_topbar.bt_icon, lv_color_hex(0x808080), 0);
    lv_obj_align_to(g_topbar.bt_icon, g_topbar.battery_icon, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    // WiFi icon
    g_topbar.wifi_icon = lv_label_create(g_topbar.bar);
    lv_label_set_text(g_topbar.wifi_icon, WIFI_SYMBOL);
    lv_obj_set_style_text_color(g_topbar.wifi_icon, lv_color_hex(0x808080), 0);
    lv_obj_align_to(g_topbar.wifi_icon, g_topbar.bt_icon, LV_ALIGN_OUT_LEFT_MID, -10, 0);

    ESP_LOGI(TAG, "Top bar initialized");
    return ESP_OK;
}

void ui_topbar_update_time(struct tm *time)
{
    if (!g_topbar.time_label || !time) {
        return;
    }

    char time_str[16];
    snprintf(time_str, sizeof(time_str), "%02d:%02d:%02d", 
             time->tm_hour, time->tm_min, time->tm_sec);
    lv_label_set_text(g_topbar.time_label, time_str);
}

void ui_topbar_update_wifi(bool connected, int8_t rssi)
{
    if (!g_topbar.wifi_icon) {
        return;
    }

    if (!connected) {
        // Disconnected - gray
        lv_obj_set_style_text_color(g_topbar.wifi_icon, lv_color_hex(0x808080), 0);
    } else {
        // Connected - color based on signal strength
        lv_color_t color;
        if (rssi > -50) {
            color = lv_color_hex(0x00FF00);  // Strong - green
        } else if (rssi > -70) {
            color = lv_color_hex(0xFFFF00);  // Medium - yellow
        } else {
            color = lv_color_hex(0xFF8800);  // Weak - orange
        }
        lv_obj_set_style_text_color(g_topbar.wifi_icon, color, 0);
    }
}

void ui_topbar_update_bluetooth(bool enabled, bool connected)
{
    if (!g_topbar.bt_icon) {
        return;
    }

    lv_color_t color;
    if (!enabled) {
        color = lv_color_hex(0x404040);  // Off - dark gray
    } else if (connected) {
        color = lv_color_hex(0x0080FF);  // Connected - blue
    } else {
        color = lv_color_hex(0x808080);  // On but not connected - gray
    }
    lv_obj_set_style_text_color(g_topbar.bt_icon, color, 0);
}

void ui_topbar_update_battery(uint8_t percent, bool charging)
{
    if (!g_topbar.battery_icon || !g_topbar.battery_label) {
        return;
    }

    // Update percentage
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%d%%", percent);
    lv_label_set_text(g_topbar.battery_label, pct_str);

    // Update color based on level
    lv_color_t color;
    if (charging) {
        color = lv_color_hex(0x00FFFF);  // Charging - cyan
    } else if (percent > 50) {
        color = lv_color_hex(0x00FF00);  // Good - green
    } else if (percent > 20) {
        color = lv_color_hex(0xFFFF00);  // Low - yellow
    } else {
        color = lv_color_hex(0xFF0000);  // Critical - red
    }
    lv_obj_set_style_text_color(g_topbar.battery_icon, color, 0);
    
    // Change symbol if charging
    if (charging) {
        lv_label_set_text(g_topbar.battery_icon, LV_SYMBOL_CHARGE);
    } else {
        lv_label_set_text(g_topbar.battery_icon, BATTERY_SYMBOL);
    }
}
