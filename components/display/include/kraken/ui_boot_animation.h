#pragma once

#include "lvgl.h"
#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Boot animation completion callback
 * Called when boot animation finishes
 */
typedef void (*ui_boot_animation_complete_cb_t)(void);

/**
 * @brief Initialize and start the boot animation
 * @param screen Main LVGL screen
 * @param complete_cb Callback to call when animation completes
 * @return ESP_OK on success
 */
esp_err_t ui_boot_animation_start(lv_obj_t *screen, ui_boot_animation_complete_cb_t complete_cb);

/**
 * @brief Stop and clean up boot animation
 */
void ui_boot_animation_stop(void);

#ifdef __cplusplus
}
#endif
