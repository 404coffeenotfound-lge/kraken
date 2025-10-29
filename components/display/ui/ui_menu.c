#include "ui_internal.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_menu";

#define MENU_ITEM_HEIGHT 60
#define TOPBAR_HEIGHT 30

typedef struct {
    const char *title;
    const char *icon;
} menu_item_config_t;

static const menu_item_config_t g_menu_items[UI_MENU_ITEM_COUNT] = {
    [UI_MENU_ITEM_AUDIO]     = {"Audio",     LV_SYMBOL_AUDIO},
    [UI_MENU_ITEM_NETWORK]   = {"Network",   LV_SYMBOL_WIFI},
    [UI_MENU_ITEM_BLUETOOTH] = {"Bluetooth", LV_SYMBOL_BLUETOOTH},
    [UI_MENU_ITEM_APPS]      = {"Apps",      LV_SYMBOL_LIST},
    [UI_MENU_ITEM_SETTINGS]  = {"Settings",  LV_SYMBOL_SETTINGS},
    [UI_MENU_ITEM_ABOUT]     = {"About",     LV_SYMBOL_WARNING},
};

static struct {
    lv_obj_t *container;
    lv_obj_t *items[UI_MENU_ITEM_COUNT];
    int selected_index;
    ui_menu_callback_t callback;
} g_menu = {0};

esp_err_t ui_menu_init(lv_obj_t *parent)
{
    if (!parent) {
        return ESP_ERR_INVALID_ARG;
    }

    // Create menu container - black background, white text
    g_menu.container = lv_obj_create(parent);
    lv_obj_set_size(g_menu.container, LV_HOR_RES, LV_VER_RES - TOPBAR_HEIGHT);
    lv_obj_set_pos(g_menu.container, 0, TOPBAR_HEIGHT);
    lv_obj_set_style_bg_color(g_menu.container, lv_color_hex(0xFFFFFF), 0);  // White = Black
    lv_obj_set_style_border_width(g_menu.container, 0, 0);
    lv_obj_set_style_pad_all(g_menu.container, 5, 0);
    lv_obj_set_style_pad_gap(g_menu.container, 5, 0);
    lv_obj_set_scroll_dir(g_menu.container, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_menu.container, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(g_menu.container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_menu.container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START);

    // Create menu items
    for (int i = 0; i < UI_MENU_ITEM_COUNT; i++) {
        g_menu.items[i] = ui_create_menu_item(g_menu.container, 
                                                g_menu_items[i].title,
                                                g_menu_items[i].icon);
    }

    // Select first item by default
    g_menu.selected_index = 0;
    ui_set_menu_item_selected(g_menu.items[0], true);

    ESP_LOGI(TAG, "Menu initialized with %d items", UI_MENU_ITEM_COUNT);
    return ESP_OK;
}

void ui_menu_set_callback(ui_menu_callback_t callback)
{
    g_menu.callback = callback;
}

void ui_menu_navigate(kraken_event_type_t direction)
{
    if (!g_menu.container) {
        return;
    }

    int old_index = g_menu.selected_index;
    int new_index = old_index;

    switch (direction) {
        case KRAKEN_EVENT_INPUT_UP:
            new_index = (old_index - 1 + UI_MENU_ITEM_COUNT) % UI_MENU_ITEM_COUNT;
            break;
            
        case KRAKEN_EVENT_INPUT_DOWN:
            new_index = (old_index + 1) % UI_MENU_ITEM_COUNT;
            break;
            
        case KRAKEN_EVENT_INPUT_LEFT:
        case KRAKEN_EVENT_INPUT_RIGHT:
            // Could be used for sub-menus or horizontal navigation
            break;
            
        default:
            return;
    }

    if (new_index != old_index) {
        // Deselect old item
        ui_set_menu_item_selected(g_menu.items[old_index], false);
        
        // Select new item
        g_menu.selected_index = new_index;
        ui_set_menu_item_selected(g_menu.items[new_index], true);
        
        // Scroll to make selected item visible
        lv_obj_scroll_to_view(g_menu.items[new_index], LV_ANIM_ON);
        
        ESP_LOGI(TAG, "Menu selection changed: %d -> %d (%s)", 
                 old_index, new_index, g_menu_items[new_index].title);
    }
}

void ui_menu_select_current(void)
{
    if (g_menu.callback) {
        ESP_LOGI(TAG, "Menu item selected: %d (%s)", 
                 g_menu.selected_index, g_menu_items[g_menu.selected_index].title);
        g_menu.callback((ui_menu_item_t)g_menu.selected_index);
    }
}

ui_menu_item_t ui_menu_get_selected(void)
{
    return (ui_menu_item_t)g_menu.selected_index;
}
