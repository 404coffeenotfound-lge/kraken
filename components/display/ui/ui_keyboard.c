#include "kraken/ui_keyboard.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_keyboard";

// Compact QWERTY layout with special characters
static const char *lowercase_keys[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=",
    "q", "w", "e", "r", "t", "y", "u", "i", "o", "p", "[", "]",
    "a", "s", "d", "f", "g", "h", "j", "k", "l", ";", "'",
    "z", "x", "c", "v", "b", "n", "m", ",", ".", "/",
    "#+=", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_LEFT, LV_SYMBOL_OK
};

static const char *uppercase_keys[] = {
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+",
    "Q", "W", "E", "R", "T", "Y", "U", "I", "O", "P", "{", "}",
    "A", "S", "D", "F", "G", "H", "J", "K", "L", ":", "\"",
    "Z", "X", "C", "V", "B", "N", "M", "<", ">", "?",
    "#+=", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_LEFT, LV_SYMBOL_OK
};

static const char *symbol_keys[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", "-", "=",
    "!", "@", "#", "$", "%", "^", "&", "*", "(", ")", "_", "+",
    "[", "]", "{", "}", "\\", "|", ";", ":", "'", "\"", "`",
    "<", ">", ",", ".", "/", "?", "~", " ", " ", " ",
    "abc", " ", LV_SYMBOL_BACKSPACE, LV_SYMBOL_LEFT, LV_SYMBOL_OK
};

static const uint8_t row_lengths[] = {12, 12, 11, 10, 5};  // Keys per row

static void update_key_selection(ui_keyboard_t *kb);

ui_keyboard_t *ui_keyboard_create(lv_obj_t *parent, lv_obj_t *textarea)
{
    ui_keyboard_t *kb = malloc(sizeof(ui_keyboard_t));
    if (!kb) {
        ESP_LOGE(TAG, "Failed to allocate keyboard");
        return NULL;
    }
    
    memset(kb, 0, sizeof(ui_keyboard_t));
    kb->textarea = textarea;
    kb->mode = KEYBOARD_MODE_LOWERCASE;
    kb->selected_row = 1;  // Start at QWERTY row
    kb->selected_col = 0;
    kb->rows = 5;
    memcpy(kb->cols, row_lengths, sizeof(row_lengths));
    
    // Create container - more compact
    kb->container = lv_obj_create(parent);
    lv_obj_set_size(kb->container, LV_HOR_RES, 140);  // Reduced from 180
    lv_obj_align(kb->container, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(kb->container, lv_color_hex(0xE0E0E0), 0);  // Light gray
    lv_obj_set_style_radius(kb->container, 0, 0);
    lv_obj_set_style_border_width(kb->container, 0, 0);
    lv_obj_set_style_pad_all(kb->container, 2, 0);
    lv_obj_clear_flag(kb->container, LV_OBJ_FLAG_SCROLLABLE);
    
    // Create keys
    const char **keys = lowercase_keys;
    uint8_t key_idx = 0;
    uint8_t y_offset = 2;
    
    for (uint8_t row = 0; row < 5; row++) {
        uint8_t num_keys = row_lengths[row];
        uint8_t key_width = 18;  // Default smaller width
        uint8_t x_offset = 2;
        
        // Center rows based on key count
        if (row == 2) x_offset += 5;   // 11-key row
        if (row == 3) x_offset += 10;  // 10-key row
        
        for (uint8_t col = 0; col < num_keys; col++) {
            lv_obj_t *btn = lv_btn_create(kb->container);
            
            // Special sizing for bottom row
            if (strcmp(keys[key_idx], " ") == 0) {
                key_width = 100;  // Much longer space bar
            } else if (strcmp(keys[key_idx], LV_SYMBOL_BACKSPACE) == 0) {
                key_width = 30;
            } else if (strcmp(keys[key_idx], LV_SYMBOL_LEFT) == 0) {
                key_width = 30;
            } else if (strcmp(keys[key_idx], LV_SYMBOL_OK) == 0) {
                key_width = 30;
            } else if (strcmp(keys[key_idx], "#+=") == 0 || strcmp(keys[key_idx], "abc") == 0) {
                key_width = 30;
            } else {
                key_width = 18;  // Regular key
            }
            
            lv_obj_set_size(btn, key_width, 25);  // Reduced height from 33
            lv_obj_set_pos(btn, x_offset, y_offset);
            lv_obj_set_style_radius(btn, 3, 0);
            lv_obj_set_style_bg_color(btn, lv_color_hex(0xFFFFFF), 0);  // White
            lv_obj_set_style_border_width(btn, 1, 0);
            lv_obj_set_style_border_color(btn, lv_color_hex(0xC0C0C0), 0);
            lv_obj_set_style_shadow_width(btn, 0, 0);  // Remove shadow for compact look
            lv_obj_set_style_pad_all(btn, 0, 0);
            
            // Label with smaller font
            lv_obj_t *label = lv_label_create(btn);
            lv_label_set_text(label, keys[key_idx]);
            lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
            lv_obj_set_style_text_font(label, &lv_font_montserrat_14, 0);  // Use available smaller font
            lv_obj_center(label);
            
            // Store key
            kb->keys[key_idx] = btn;
            strncpy(kb->key_labels[key_idx], keys[key_idx], 3);
            kb->key_labels[key_idx][3] = '\0';
            
            x_offset += key_width + 2;
            key_idx++;
        }
        
        y_offset += 27;  // Reduced from 36
    }
    
    update_key_selection(kb);
    
    ESP_LOGI(TAG, "Keyboard created with %d keys", key_idx);
    return kb;
}

void ui_keyboard_delete(ui_keyboard_t *kb)
{
    if (kb) {
        if (kb->container) {
            lv_obj_del(kb->container);
        }
        free(kb);
    }
}

void ui_keyboard_set_ok_callback(ui_keyboard_t *kb, keyboard_callback_t callback, void *user_data)
{
    if (kb) {
        kb->ok_callback = callback;
        kb->user_data = user_data;
    }
}

void ui_keyboard_set_cancel_callback(ui_keyboard_t *kb, keyboard_callback_t callback, void *user_data)
{
    if (kb) {
        kb->cancel_callback = callback;
        kb->user_data = user_data;
    }
}

static void update_key_selection(ui_keyboard_t *kb)
{
    // Calculate absolute key index from row/col
    uint8_t key_idx = 0;
    for (uint8_t r = 0; r < kb->selected_row; r++) {
        key_idx += kb->cols[r];
    }
    key_idx += kb->selected_col;
    
    // Update all keys
    uint8_t idx = 0;
    for (uint8_t row = 0; row < kb->rows; row++) {
        for (uint8_t col = 0; col < kb->cols[row]; col++) {
            if (idx == key_idx) {
                // Selected key - blue background
                lv_obj_set_style_bg_color(kb->keys[idx], lv_color_hex(0x5599FF), 0);
                lv_obj_t *label = lv_obj_get_child(kb->keys[idx], 0);
                if (label) {
                    lv_obj_set_style_text_color(label, lv_color_hex(0xFFFFFF), 0);  // White text
                }
            } else {
                // Normal key - white background
                lv_obj_set_style_bg_color(kb->keys[idx], lv_color_hex(0xFFFFFF), 0);
                lv_obj_t *label = lv_obj_get_child(kb->keys[idx], 0);
                if (label) {
                    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);  // Black text
                }
            }
            idx++;
        }
    }
}

void ui_keyboard_handle_input(ui_keyboard_t *kb, kraken_event_type_t input)
{
    if (!kb) return;
    
    if (input == KRAKEN_EVENT_INPUT_UP) {
        // Move up
        if (kb->selected_row > 0) {
            kb->selected_row--;
            // Clamp column to valid range
            if (kb->selected_col >= kb->cols[kb->selected_row]) {
                kb->selected_col = kb->cols[kb->selected_row] - 1;
            }
            update_key_selection(kb);
        }
        ESP_LOGI(TAG, "Selection: row=%d, col=%d", kb->selected_row, kb->selected_col);
        
    } else if (input == KRAKEN_EVENT_INPUT_DOWN) {
        // Move down
        if (kb->selected_row < kb->rows - 1) {
            kb->selected_row++;
            // Clamp column to valid range
            if (kb->selected_col >= kb->cols[kb->selected_row]) {
                kb->selected_col = kb->cols[kb->selected_row] - 1;
            }
            update_key_selection(kb);
        }
        ESP_LOGI(TAG, "Selection: row=%d, col=%d", kb->selected_row, kb->selected_col);
        
    } else if (input == KRAKEN_EVENT_INPUT_RIGHT) {
        // Move right
        if (kb->selected_col < kb->cols[kb->selected_row] - 1) {
            kb->selected_col++;
            update_key_selection(kb);
        }
        ESP_LOGI(TAG, "Selection: row=%d, col=%d", kb->selected_row, kb->selected_col);
        
    } else if (input == KRAKEN_EVENT_INPUT_LEFT) {
        // Move left
        if (kb->selected_col > 0) {
            kb->selected_col--;
            update_key_selection(kb);
        }
        ESP_LOGI(TAG, "Selection: row=%d, col=%d", kb->selected_row, kb->selected_col);
        
    } else if (input == KRAKEN_EVENT_INPUT_CENTER) {
        // Press current key
        uint8_t key_idx = 0;
        for (uint8_t r = 0; r < kb->selected_row; r++) {
            key_idx += kb->cols[r];
        }
        key_idx += kb->selected_col;
        
        const char *key = kb->key_labels[key_idx];
        ESP_LOGI(TAG, "Pressed key: '%s'", key);
        
        if (strcmp(key, LV_SYMBOL_OK) == 0) {
            // Call OK callback
            if (kb->ok_callback) {
                const char *text = kb->textarea ? lv_textarea_get_text(kb->textarea) : "";
                kb->ok_callback(text, kb->user_data);
            }
        } else if (strcmp(key, LV_SYMBOL_LEFT) == 0) {
            // Back button - call cancel callback
            if (kb->cancel_callback) {
                const char *text = kb->textarea ? lv_textarea_get_text(kb->textarea) : "";
                kb->cancel_callback(text, kb->user_data);
            }
        } else if (strcmp(key, LV_SYMBOL_BACKSPACE) == 0) {
            // Delete character
            if (kb->textarea) {
                lv_textarea_delete_char(kb->textarea);
            }
        } else if (strcmp(key, "#+=") == 0 || strcmp(key, "abc") == 0) {
            // Switch between modes
            if (kb->mode == KEYBOARD_MODE_LOWERCASE) {
                ui_keyboard_set_mode(kb, KEYBOARD_MODE_UPPERCASE);
            } else if (kb->mode == KEYBOARD_MODE_UPPERCASE) {
                ui_keyboard_set_mode(kb, KEYBOARD_MODE_SYMBOLS);
            } else {
                ui_keyboard_set_mode(kb, KEYBOARD_MODE_LOWERCASE);
            }
        } else if (strcmp(key, " ") == 0) {
            // Space
            if (kb->textarea) {
                lv_textarea_add_text(kb->textarea, " ");
            }
        } else {
            // Regular character
            if (kb->textarea) {
                lv_textarea_add_text(kb->textarea, key);
            }
        }
    }
}

void ui_keyboard_set_mode(ui_keyboard_t *kb, ui_keyboard_mode_t mode)
{
    if (!kb) return;
    
    kb->mode = mode;
    
    const char **keys;
    switch (mode) {
        case KEYBOARD_MODE_UPPERCASE:
            keys = uppercase_keys;
            break;
        case KEYBOARD_MODE_SYMBOLS:
            keys = symbol_keys;
            break;
        case KEYBOARD_MODE_LOWERCASE:
        default:
            keys = lowercase_keys;
            break;
    }
    
    // Update all key labels
    uint8_t idx = 0;
    for (uint8_t row = 0; row < kb->rows; row++) {
        for (uint8_t col = 0; col < kb->cols[row]; col++) {
            lv_obj_t *label = lv_obj_get_child(kb->keys[idx], 0);
            if (label) {
                lv_label_set_text(label, keys[idx]);
                strncpy(kb->key_labels[idx], keys[idx], 3);
                kb->key_labels[idx][3] = '\0';
            }
            idx++;
        }
    }
    
    ESP_LOGI(TAG, "Keyboard mode changed to %d", mode);
}

const char *ui_keyboard_get_text(ui_keyboard_t *kb)
{
    if (kb && kb->textarea) {
        return lv_textarea_get_text(kb->textarea);
    }
    return "";
}
