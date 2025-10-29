#include "ui_boot_animation.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <math.h>

static const char *TAG = "ui_boot";

#define KRAKEN_PIXEL_SIZE 8
#define KRAKEN_PIXELS_X 8
#define KRAKEN_PIXELS_Y 8
#define ANIMATION_DURATION_MS 3000
#define PIXEL_UPDATE_INTERVAL_MS 50

// Kraken logo pattern (8x8 pixel art)
// K = Kraken pixel, . = empty
static const char kraken_pattern[KRAKEN_PIXELS_Y][KRAKEN_PIXELS_X + 1] = {
    "K......K",
    ".K....K.",
    "..K..K..",
    "...KK...",
    "...KK...",
    "..K..K..",
    ".K....K.",
    "K......K"
};

static struct {
    lv_obj_t *screen;
    lv_obj_t *pixels[KRAKEN_PIXELS_Y][KRAKEN_PIXELS_X];
    lv_obj_t *label;
    lv_timer_t *anim_timer;
    ui_boot_animation_complete_cb_t complete_cb;
    uint32_t frame;
    bool running;
} g_boot = {0};

static void create_pixel_grid(void)
{
    // Calculate center position
    int grid_width = KRAKEN_PIXELS_X * (KRAKEN_PIXEL_SIZE + 2);
    int grid_height = KRAKEN_PIXELS_Y * (KRAKEN_PIXEL_SIZE + 2);
    int start_x = (lv_obj_get_width(g_boot.screen) - grid_width) / 2;
    int start_y = (lv_obj_get_height(g_boot.screen) - grid_height) / 2 - 20;

    // Create pixel objects
    for (int y = 0; y < KRAKEN_PIXELS_Y; y++) {
        for (int x = 0; x < KRAKEN_PIXELS_X; x++) {
            lv_obj_t *pixel = lv_obj_create(g_boot.screen);
            lv_obj_set_size(pixel, KRAKEN_PIXEL_SIZE, KRAKEN_PIXEL_SIZE);
            lv_obj_set_pos(pixel, 
                          start_x + x * (KRAKEN_PIXEL_SIZE + 2),
                          start_y + y * (KRAKEN_PIXEL_SIZE + 2));
            
            // Style - white background with light gray border
            lv_obj_set_style_bg_color(pixel, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(pixel, 1, 0);
            lv_obj_set_style_border_color(pixel, lv_color_hex(0xD0D0D0), 0);
            lv_obj_set_style_radius(pixel, 2, 0);
            
            // Initially hidden if not part of pattern
            if (kraken_pattern[y][x] != 'K') {
                lv_obj_add_flag(pixel, LV_OBJ_FLAG_HIDDEN);
            }
            
            g_boot.pixels[y][x] = pixel;
        }
    }
}

static void animation_tick(lv_timer_t *timer)
{
    (void)timer;
    
    if (!g_boot.running) {
        return;
    }

    g_boot.frame++;
    
    // Animation phases:
    // 1. Fade in pixels with rainbow colors (0-1000ms)
    // 2. Dancing/pulsing effect (1000-2500ms)
    // 3. Fade to final color (2500-3000ms)
    
    uint32_t time_ms = g_boot.frame * PIXEL_UPDATE_INTERVAL_MS;
    
    if (time_ms < 1000) {
        // Phase 1: Fade in space gray
        for (int y = 0; y < KRAKEN_PIXELS_Y; y++) {
            for (int x = 0; x < KRAKEN_PIXELS_X; x++) {
                if (kraken_pattern[y][x] == 'K') {
                    lv_obj_clear_flag(g_boot.pixels[y][x], LV_OBJ_FLAG_HIDDEN);
                    
                    // Space gray color
                    lv_obj_set_style_bg_color(g_boot.pixels[y][x], lv_color_hex(0x3C3C3C), 0);
                    
                    // Fade in opacity
                    lv_opa_t opa = (time_ms * 255) / 1000;
                    lv_obj_set_style_bg_opa(g_boot.pixels[y][x], opa, 0);
                }
            }
        }
    } else if (time_ms < 2500) {
        // Phase 2: Pulsing effect (subtle)
        for (int y = 0; y < KRAKEN_PIXELS_Y; y++) {
            for (int x = 0; x < KRAKEN_PIXELS_X; x++) {
                if (kraken_pattern[y][x] == 'K') {
                    // Subtle pulse
                    float phase = (time_ms - 1000) / 1500.0f * M_PI * 2;
                    float wave = sin(phase + (x + y) * 0.3f);
                    
                    int size = KRAKEN_PIXEL_SIZE + (int)(wave * 1);  // Smaller pulse
                    lv_obj_set_size(g_boot.pixels[y][x], size, size);
                    
                    // Keep space gray
                    lv_obj_set_style_bg_color(g_boot.pixels[y][x], lv_color_hex(0x3C3C3C), 0);
                    lv_obj_set_style_bg_opa(g_boot.pixels[y][x], LV_OPA_COVER, 0);
                }
            }
        }
    } else if (time_ms < ANIMATION_DURATION_MS) {
        // Phase 3: Settle to final state
        for (int y = 0; y < KRAKEN_PIXELS_Y; y++) {
            for (int x = 0; x < KRAKEN_PIXELS_X; x++) {
                if (kraken_pattern[y][x] == 'K') {
                    // Reset size
                    lv_obj_set_size(g_boot.pixels[y][x], KRAKEN_PIXEL_SIZE, KRAKEN_PIXEL_SIZE);
                    
                    // Final space gray
                    lv_obj_set_style_bg_color(g_boot.pixels[y][x], lv_color_hex(0x3C3C3C), 0);
                    lv_obj_set_style_bg_opa(g_boot.pixels[y][x], LV_OPA_COVER, 0);
                    
                    // Subtle border
                    lv_obj_set_style_border_color(g_boot.pixels[y][x], lv_color_hex(0x3C3C3C), 0);
                    lv_obj_set_style_border_width(g_boot.pixels[y][x], 1, 0);
                }
            }
        }
    } else {
        // Animation complete
        ESP_LOGI(TAG, "Boot animation complete");
        ui_boot_animation_stop();
        
        if (g_boot.complete_cb) {
            g_boot.complete_cb();
        }
    }
}

esp_err_t ui_boot_animation_start(lv_obj_t *screen, ui_boot_animation_complete_cb_t complete_cb)
{
    if (!screen) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Starting boot animation");

    g_boot.screen = screen;
    g_boot.complete_cb = complete_cb;
    g_boot.frame = 0;
    g_boot.running = true;

    // Set white background (iPod style)
    lv_obj_set_style_bg_color(screen, lv_color_hex(0xFFFFFF), 0);

    // Create pixel grid
    create_pixel_grid();

    // Create "Kraken OS" label below the animation - space gray
    g_boot.label = lv_label_create(screen);
    lv_label_set_text(g_boot.label, "Kraken OS");
    lv_obj_set_style_text_color(g_boot.label, lv_color_hex(0x3C3C3C), 0);  // Space gray
    lv_obj_align(g_boot.label, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Start animation timer
    g_boot.anim_timer = lv_timer_create(animation_tick, PIXEL_UPDATE_INTERVAL_MS, NULL);

    return ESP_OK;
}

void ui_boot_animation_stop(void)
{
    if (!g_boot.running) {
        return;
    }

    g_boot.running = false;

    // Stop timer
    if (g_boot.anim_timer) {
        lv_timer_del(g_boot.anim_timer);
        g_boot.anim_timer = NULL;
    }

    // Clean up pixels
    for (int y = 0; y < KRAKEN_PIXELS_Y; y++) {
        for (int x = 0; x < KRAKEN_PIXELS_X; x++) {
            if (g_boot.pixels[y][x]) {
                lv_obj_del(g_boot.pixels[y][x]);
                g_boot.pixels[y][x] = NULL;
            }
        }
    }

    // Clean up label
    if (g_boot.label) {
        lv_obj_del(g_boot.label);
        g_boot.label = NULL;
    }

    ESP_LOGI(TAG, "Boot animation stopped");
}
