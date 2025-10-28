#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "lvgl.h"
#include "kraken/kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*display_task_cb_t)(lv_obj_t *parent);

esp_err_t display_service_init(void);
esp_err_t display_service_deinit(void);

lv_obj_t *display_create_window(const char *title);
void display_delete_window(lv_obj_t *win);

lv_obj_t *display_create_textbox(lv_obj_t *parent, const char *placeholder);
lv_obj_t *display_create_button(lv_obj_t *parent, const char *label, lv_event_cb_t callback);
lv_obj_t *display_create_label(lv_obj_t *parent, const char *text);
lv_obj_t *display_create_list(lv_obj_t *parent);
lv_obj_t *display_create_virtual_keyboard(lv_obj_t *parent, lv_obj_t *target);

void display_show_notification(const char *message, uint32_t duration_ms);

void display_lock(void);
void display_unlock(void);

esp_err_t display_handle_input(kraken_event_type_t input_event);

#ifdef __cplusplus
}
#endif
