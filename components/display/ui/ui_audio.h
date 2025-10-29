#pragma once

#include "lvgl.h"
#include "kraken/kernel.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Create the Audio menu screen
 * @param parent Parent LVGL object to attach the screen to
 * @return Created Audio screen object
 */
lv_obj_t *ui_audio_screen_create(lv_obj_t *parent);

/**
 * @brief Show the Audio menu screen
 */
void ui_audio_screen_show(void);

/**
 * @brief Hide the Audio menu screen
 */
void ui_audio_screen_hide(void);

/**
 * @brief Handle input events for Audio screen navigation
 * @param input Input event type
 */
void ui_audio_handle_input(kraken_event_type_t input);

/**
 * @brief Delete the Audio screen and cleanup resources
 */
void ui_audio_screen_delete(void);

#ifdef __cplusplus
}
#endif
