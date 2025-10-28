#include "kraken/display_service.h"
#include "kraken/kernel.h"
#include "kraken/bsp.h"
#include "ui/ui_internal.h"
#include "esp_lvgl_port.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <string.h>

static const char *TAG = "display_service";

#define UI_UPDATE_PERIOD_MS 1000  // Update UI every second

static struct {
    bool initialized;
    lv_display_t *disp;
    esp_lcd_panel_handle_t panel_handle;
    lv_obj_t *screen;
    esp_timer_handle_t update_timer;
} g_display = {0};

static void ui_update_timer_callback(void *arg);

esp_err_t display_service_init(void)
{
    if (g_display.initialized) {
        return ESP_OK;
    }

    const board_display_config_t *cfg = board_support_get_display_config();

    spi_bus_config_t buscfg = {
        .mosi_io_num = cfg->pin_mosi,
        .miso_io_num = -1,
        .sclk_io_num = cfg->pin_sclk,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = cfg->hor_res * cfg->ver_res * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(cfg->host, &buscfg, cfg->dma_channel));

    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = cfg->pin_dc,
        .cs_gpio_num = cfg->pin_cs,
        .pclk_hz = 40 * 1000 * 1000,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
        .spi_mode = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)cfg->host, 
                                              &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = cfg->pin_rst,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &g_display.panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(g_display.panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(g_display.panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(g_display.panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(g_display.panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(g_display.panel_handle, false, true));
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(g_display.panel_handle, 0, 0));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(g_display.panel_handle, true));

    const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();
    ESP_ERROR_CHECK(lvgl_port_init(&lvgl_cfg));

    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = g_display.panel_handle,
        .buffer_size = cfg->hor_res * 50,
        .double_buffer = true,
        .hres = cfg->hor_res,
        .vres = cfg->ver_res,
        .monochrome = false,
        .rotation = {
            .swap_xy = false,
            .mirror_x = false,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .buff_spiram = false,
        }
    };
    g_display.disp = lvgl_port_add_disp(&disp_cfg);

    g_display.screen = lv_screen_active();
    lv_obj_set_style_bg_color(g_display.screen, lv_color_hex(0x000000), 0);

    // Initialize UI Manager with modular UI components
    ESP_ERROR_CHECK(ui_manager_init(g_display.screen));

    // Create periodic timer for UI updates
    esp_timer_create_args_t timer_args = {
        .callback = ui_update_timer_callback,
        .name = "ui_update",
        .arg = NULL
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_display.update_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(g_display.update_timer, UI_UPDATE_PERIOD_MS * 1000));

    g_display.initialized = true;
    ESP_LOGI(TAG, "Display service initialized with modular UI");
    return ESP_OK;
}

static void ui_update_timer_callback(void *arg)
{
    (void)arg;
    // This will be called every second to update the UI
    extern void ui_manager_periodic_update(void);
    ui_manager_periodic_update();
}

esp_err_t display_service_deinit(void)
{
    if (!g_display.initialized) {
        return ESP_OK;
    }

    // Stop update timer
    if (g_display.update_timer) {
        esp_timer_stop(g_display.update_timer);
        esp_timer_delete(g_display.update_timer);
    }

    // Deinit UI manager
    ui_manager_deinit();

    lvgl_port_remove_disp(g_display.disp);
    lvgl_port_deinit();

    esp_lcd_panel_del(g_display.panel_handle);

    g_display.initialized = false;
    ESP_LOGI(TAG, "Display service deinitialized");
    return ESP_OK;
}

lv_obj_t *display_create_window(const char *title)
{
    lv_obj_t *win = lv_obj_create(g_display.screen);
    lv_obj_set_size(win, LV_HOR_RES, LV_VER_RES);
    lv_obj_center(win);

    if (title) {
        lv_obj_t *label = lv_label_create(win);
        lv_label_set_text(label, title);
        lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 10);
    }

    return win;
}

void display_delete_window(lv_obj_t *win)
{
    if (win) {
        lv_obj_delete(win);
    }
}

lv_obj_t *display_create_textbox(lv_obj_t *parent, const char *placeholder)
{
    lv_obj_t *ta = lv_textarea_create(parent ? parent : g_display.screen);
    lv_textarea_set_one_line(ta, true);
    if (placeholder) {
        lv_textarea_set_placeholder_text(ta, placeholder);
    }
    return ta;
}

lv_obj_t *display_create_button(lv_obj_t *parent, const char *label, lv_event_cb_t callback)
{
    lv_obj_t *btn = lv_button_create(parent ? parent : g_display.screen);
    
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label ? label : "Button");
    lv_obj_center(lbl);

    if (callback) {
        lv_obj_add_event_cb(btn, callback, LV_EVENT_CLICKED, NULL);
    }

    return btn;
}

lv_obj_t *display_create_label(lv_obj_t *parent, const char *text)
{
    lv_obj_t *label = lv_label_create(parent ? parent : g_display.screen);
    lv_label_set_text(label, text ? text : "");
    return label;
}

lv_obj_t *display_create_list(lv_obj_t *parent)
{
    lv_obj_t *list = lv_list_create(parent ? parent : g_display.screen);
    lv_obj_set_size(list, LV_HOR_RES - 20, LV_VER_RES - 60);
    return list;
}

lv_obj_t *display_create_virtual_keyboard(lv_obj_t *parent, lv_obj_t *target)
{
    lv_obj_t *kb = lv_keyboard_create(parent ? parent : g_display.screen);
    if (target) {
        lv_keyboard_set_textarea(kb, target);
    }
    return kb;
}

void display_show_notification(const char *message, uint32_t duration_ms)
{
    if (!message) return;

    lv_obj_t *mbox = lv_msgbox_create(g_display.screen);
    lv_msgbox_add_text(mbox, message);
    lv_obj_center(mbox);
}

void display_lock(void)
{
    lvgl_port_lock(0);
}

void display_unlock(void)
{
    lvgl_port_unlock();
}

esp_err_t display_handle_input(kraken_event_type_t input_event)
{
    ESP_LOGI(TAG, "Input event: %d", input_event);
    return ESP_OK;
}
