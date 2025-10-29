#pragma once

#include "lvgl.h"
#include "kraken/kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEYBOARD_MODE_LOWERCASE = 0,
    KEYBOARD_MODE_UPPERCASE,
    KEYBOARD_MODE_SYMBOLS,
} ui_keyboard_mode_t;

typedef void (*keyboard_callback_t)(const char *text, void *user_data);

typedef struct {
    lv_obj_t *container;
    lv_obj_t *textarea;
    keyboard_callback_t ok_callback;
    keyboard_callback_t cancel_callback;
    void *user_data;
    
    ui_keyboard_mode_t mode;
    uint8_t selected_row;
    uint8_t selected_col;
    uint8_t rows;
    uint8_t cols[5];  // Columns per row (max 5 rows)
    
    lv_obj_t *keys[50];  // Max 50 keys
    char key_labels[50][4];  // Label for each key
} ui_keyboard_t;

/**
 * Create a custom virtual keyboard
 * @param parent Parent object
 * @param textarea Textarea to bind to (optional, can be NULL)
 * @return Keyboard object
 */
ui_keyboard_t *ui_keyboard_create(lv_obj_t *parent, lv_obj_t *textarea);

/**
 * Delete keyboard
 * @param kb Keyboard object
 */
void ui_keyboard_delete(ui_keyboard_t *kb);

/**
 * Set OK callback (called when user presses OK/Enter)
 * @param kb Keyboard object
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void ui_keyboard_set_ok_callback(ui_keyboard_t *kb, keyboard_callback_t callback, void *user_data);

/**
 * Set Cancel callback (called when user presses Cancel)
 * @param kb Keyboard object
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void ui_keyboard_set_cancel_callback(ui_keyboard_t *kb, keyboard_callback_t callback, void *user_data);

/**
 * Handle input from hardware buttons
 * @param kb Keyboard object
 * @param input Input event type
 */
void ui_keyboard_handle_input(ui_keyboard_t *kb, kraken_event_type_t input);

/**
 * Set keyboard mode (lowercase/uppercase/symbols)
 * @param kb Keyboard object
 * @param mode Keyboard mode
 */
void ui_keyboard_set_mode(ui_keyboard_t *kb, ui_keyboard_mode_t mode);

/**
 * Get current text from bound textarea
 * @param kb Keyboard object
 * @return Text string (do not free)
 */
const char *ui_keyboard_get_text(ui_keyboard_t *kb);

#ifdef __cplusplus
}
#endif
