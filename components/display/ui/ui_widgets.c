#include "ui_internal.h"
#include "esp_log.h"

static const char *TAG = "ui_widgets";

lv_obj_t *ui_create_icon_label(lv_obj_t *parent, const char *symbol, const char *text)
{
    lv_obj_t *container = lv_obj_create(parent);
    lv_obj_set_size(container, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(container, 0, 0);
    lv_obj_set_flex_flow(container, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon - use default font (symbols work with default)
    lv_obj_t *icon = lv_label_create(container);
    lv_label_set_text(icon, symbol);

    // Text
    lv_obj_t *label = lv_label_create(container);
    lv_label_set_text(label, text);
    lv_obj_set_style_pad_left(label, 10, 0);

    return container;
}

lv_obj_t *ui_create_menu_item(lv_obj_t *parent, const char *title, const char *icon)
{
    lv_obj_t *item = lv_obj_create(parent);
    lv_obj_set_width(item, LV_HOR_RES - 20);
    lv_obj_set_height(item, 50);
    lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);  // White = Black
    lv_obj_set_style_radius(item, 0, 0);  // Rectangle, no radius
    lv_obj_set_style_border_width(item, 1, 0);  // Border
    lv_obj_set_style_border_color(item, lv_color_hex(0x7F7F7F), 0);  // Gray border
    lv_obj_set_style_pad_all(item, 12, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon - white
    lv_obj_t *icon_label = lv_label_create(item);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0x000000), 0);  // Black = White

    // Title - white
    lv_obj_t *title_label = lv_label_create(item);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x000000), 0);  // Black = White
    lv_obj_set_style_pad_left(title_label, 15, 0);

    return item;
}

void ui_set_menu_item_selected(lv_obj_t *item, bool selected)
{
    if (!item) {
        return;
    }

    if (selected) {
        // Selected - darker gray background
        lv_obj_set_style_bg_color(item, lv_color_hex(0xC0C0C0), 0);  // Gray
        lv_obj_set_style_border_color(item, lv_color_hex(0x000000), 0);  // White border
        lv_obj_set_style_border_width(item, 2, 0);
    } else {
        // Normal - black background
        lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);  // Black
        lv_obj_set_style_border_color(item, lv_color_hex(0x7F7F7F), 0);  // Gray border
        lv_obj_set_style_border_width(item, 1, 0);
    }
}
