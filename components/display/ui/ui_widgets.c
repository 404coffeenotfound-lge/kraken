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
    lv_obj_set_size(item, LV_PCT(95), 50);
    lv_obj_set_style_bg_color(item, lv_color_hex(0x222222), 0);
    lv_obj_set_style_border_color(item, lv_color_hex(0x444444), 0);
    lv_obj_set_style_border_width(item, 1, 0);
    lv_obj_set_style_radius(item, 8, 0);
    lv_obj_set_style_pad_all(item, 10, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(item, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(item, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Icon - use default font (symbols work fine)
    lv_obj_t *icon_label = lv_label_create(item);
    lv_label_set_text(icon_label, icon);
    lv_obj_set_style_text_color(icon_label, lv_color_hex(0xFFFFFF), 0);

    // Title
    lv_obj_t *title_label = lv_label_create(item);
    lv_label_set_text(title_label, title);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_pad_left(title_label, 15, 0);

    return item;
}

void ui_set_menu_item_selected(lv_obj_t *item, bool selected)
{
    if (!item) {
        return;
    }

    if (selected) {
        // Highlight selected item
        lv_obj_set_style_bg_color(item, lv_color_hex(0x0080FF), 0);
        lv_obj_set_style_border_color(item, lv_color_hex(0x00AAFF), 0);
        lv_obj_set_style_border_width(item, 2, 0);
    } else {
        // Normal state
        lv_obj_set_style_bg_color(item, lv_color_hex(0x222222), 0);
        lv_obj_set_style_border_color(item, lv_color_hex(0x444444), 0);
        lv_obj_set_style_border_width(item, 1, 0);
    }
}
