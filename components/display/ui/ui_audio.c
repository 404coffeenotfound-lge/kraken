#include "ui_internal.h"
#include "kraken/kernel.h"
#include "kraken/audio_service.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ui_audio";

#define TOPBAR_HEIGHT 30

typedef enum {
    FOCUS_BACK_BUTTON = 0,
    FOCUS_PLAY_PAUSE,
    FOCUS_COUNT
} audio_focus_t;

static struct {
    lv_obj_t *screen;
    lv_obj_t *back_button;
    lv_obj_t *volume_label;
    lv_obj_t *status_label;
    lv_obj_t *play_pause_button;
    lv_obj_t *notification;
    lv_timer_t *notification_timer;
    
    audio_focus_t focus;
    int volume;
    bool is_playing;
} g_audio = {0};

static void show_notification(const char *text, uint32_t duration_ms);

static void notification_timer_cb(lv_timer_t *timer)
{
    if (g_audio.notification) {
        lv_obj_add_flag(g_audio.notification, LV_OBJ_FLAG_HIDDEN);
    }
    g_audio.notification_timer = NULL;  // Clear the timer pointer
}

static void show_notification(const char *text, uint32_t duration_ms)
{
    if (!g_audio.notification) {
        return;
    }
    
    lv_obj_t *label = lv_obj_get_child(g_audio.notification, 0);
    if (label) {
        lv_label_set_text(label, text);
    }
    
    lv_obj_clear_flag(g_audio.notification, LV_OBJ_FLAG_HIDDEN);
    
    // Delete old timer if exists
    if (g_audio.notification_timer) {
        lv_timer_delete(g_audio.notification_timer);
        g_audio.notification_timer = NULL;
    }
    
    // Create new one-shot timer
    g_audio.notification_timer = lv_timer_create(notification_timer_cb, duration_ms, NULL);
    lv_timer_set_repeat_count(g_audio.notification_timer, 1);
}

static void update_volume_display(void)
{
    if (g_audio.volume_label) {
        char buf[32];
        snprintf(buf, sizeof(buf), "Volume: %d%%", g_audio.volume);
        lv_label_set_text(g_audio.volume_label, buf);
    }
}

static void update_focus(void)
{
    // Reset all styles
    if (g_audio.back_button) {
        lv_obj_set_style_bg_color(g_audio.back_button, lv_color_hex(0xFFFFFF), 0);
    }
    if (g_audio.play_pause_button) {
        lv_obj_set_style_bg_color(g_audio.play_pause_button, lv_color_hex(0xFFFFFF), 0);
    }
    
    // Highlight focused item
    switch (g_audio.focus) {
        case FOCUS_BACK_BUTTON:
            if (g_audio.back_button) {
                lv_obj_set_style_bg_color(g_audio.back_button, lv_color_hex(0xFFE0E0), 0);
            }
            break;
        case FOCUS_PLAY_PAUSE:
            if (g_audio.play_pause_button) {
                lv_obj_set_style_bg_color(g_audio.play_pause_button, lv_color_hex(0xE0FFE0), 0);
            }
            break;
        default:
            break;
    }
}

lv_obj_t *ui_audio_screen_create(lv_obj_t *parent)
{
    g_audio.volume = 50; // Default volume 50%
    g_audio.is_playing = false;
    g_audio.focus = FOCUS_BACK_BUTTON;
    
    // Sync volume with audio service
    audio_set_volume(g_audio.volume);
    
    // Create main screen (hidden by default)
    g_audio.screen = lv_obj_create(parent);
    lv_obj_set_size(g_audio.screen, LV_HOR_RES, LV_VER_RES - TOPBAR_HEIGHT);
    lv_obj_set_pos(g_audio.screen, 0, TOPBAR_HEIGHT);
    lv_obj_set_style_bg_color(g_audio.screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_audio.screen, 0, 0);
    lv_obj_set_style_pad_all(g_audio.screen, 10, 0);
    lv_obj_set_flex_flow(g_audio.screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_audio.screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(g_audio.screen, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(g_audio.screen, LV_OBJ_FLAG_HIDDEN);
    
    // Back button
    g_audio.back_button = lv_obj_create(g_audio.screen);
    lv_obj_set_size(g_audio.back_button, LV_HOR_RES - 40, 45);
    lv_obj_set_style_bg_color(g_audio.back_button, lv_color_hex(0xFFE0E0), 0);
    lv_obj_set_style_radius(g_audio.back_button, 5, 0);
    
    lv_obj_t *back_label = lv_label_create(g_audio.back_button);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT " Back");
    lv_obj_set_style_text_color(back_label, lv_color_hex(0x000000), 0);
    lv_obj_center(back_label);
    
    // Volume label
    g_audio.volume_label = lv_label_create(g_audio.screen);
    lv_label_set_text(g_audio.volume_label, "Volume: 50%");
    lv_obj_set_style_text_color(g_audio.volume_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_top(g_audio.volume_label, 20, 0);
    
    // Status label
    g_audio.status_label = lv_label_create(g_audio.screen);
    lv_label_set_text(g_audio.status_label, "Status: Ready");
    lv_obj_set_style_text_color(g_audio.status_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_pad_top(g_audio.status_label, 10, 0);
    
    // Play/Pause button
    g_audio.play_pause_button = lv_obj_create(g_audio.screen);
    lv_obj_set_size(g_audio.play_pause_button, LV_HOR_RES - 40, 55);
    lv_obj_set_style_bg_color(g_audio.play_pause_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_audio.play_pause_button, 5, 0);
    lv_obj_set_style_pad_top(g_audio.play_pause_button, 20, 0);
    
    lv_obj_t *play_label = lv_label_create(g_audio.play_pause_button);
    lv_label_set_text(play_label, LV_SYMBOL_PLAY " Play");
    lv_obj_set_style_text_color(play_label, lv_color_hex(0x000000), 0);
    lv_obj_center(play_label);
    
    // Notification panel
    g_audio.notification = lv_obj_create(g_audio.screen);
    lv_obj_set_size(g_audio.notification, LV_HOR_RES - 40, 40);
    lv_obj_set_style_bg_color(g_audio.notification, lv_color_hex(0xFFFFC0), 0);
    lv_obj_set_style_radius(g_audio.notification, 5, 0);
    lv_obj_set_style_pad_top(g_audio.notification, 20, 0);
    lv_obj_add_flag(g_audio.notification, LV_OBJ_FLAG_HIDDEN);
    
    lv_obj_t *notif_label = lv_label_create(g_audio.notification);
    lv_label_set_text(notif_label, "");
    lv_obj_set_style_text_color(notif_label, lv_color_hex(0x000000), 0);
    lv_obj_center(notif_label);
    
    ESP_LOGI(TAG, "Audio screen created");
    return g_audio.screen;
}

void ui_audio_screen_show(void)
{
    if (g_audio.screen) {
        lv_obj_clear_flag(g_audio.screen, LV_OBJ_FLAG_HIDDEN);
        g_audio.focus = FOCUS_BACK_BUTTON;
        update_focus();
        ESP_LOGI(TAG, "Audio screen shown");
    }
}

void ui_audio_screen_hide(void)
{
    if (g_audio.screen) {
        lv_obj_add_flag(g_audio.screen, LV_OBJ_FLAG_HIDDEN);
        ESP_LOGI(TAG, "Audio screen hidden");
    }
}

void ui_audio_handle_input(kraken_event_type_t input)
{
    switch (input) {
        case KRAKEN_EVENT_INPUT_UP:
            if (g_audio.focus > FOCUS_BACK_BUTTON) {
                g_audio.focus--;
                update_focus();
                ESP_LOGI(TAG, "Focus: %d", g_audio.focus);
            }
            break;
            
        case KRAKEN_EVENT_INPUT_DOWN:
            if (g_audio.focus < FOCUS_PLAY_PAUSE) {
                g_audio.focus++;
                update_focus();
                ESP_LOGI(TAG, "Focus: %d", g_audio.focus);
            }
            break;
            
        case KRAKEN_EVENT_INPUT_LEFT:
            // Volume down
            if (g_audio.volume > 0) {
                g_audio.volume = (g_audio.volume >= 10) ? g_audio.volume - 10 : 0;
                audio_set_volume(g_audio.volume);  // Update audio service
                update_volume_display();
                show_notification("Volume decreased", 1500);
            }
            break;
            
        case KRAKEN_EVENT_INPUT_RIGHT:
            // Volume up
            if (g_audio.volume < 100) {
                g_audio.volume = (g_audio.volume <= 90) ? g_audio.volume + 10 : 100;
                audio_set_volume(g_audio.volume);  // Update audio service
                update_volume_display();
                show_notification("Volume increased", 1500);
            }
            break;
            
        case KRAKEN_EVENT_INPUT_CENTER:
            if (g_audio.focus == FOCUS_BACK_BUTTON) {
                ESP_LOGI(TAG, "Back button pressed");
                extern void ui_manager_exit_submenu(void);
                ui_manager_exit_submenu();
            } else if (g_audio.focus == FOCUS_PLAY_PAUSE) {
                g_audio.is_playing = !g_audio.is_playing;
                if (g_audio.is_playing) {
                    // Check WiFi status
                    extern bool wifi_service_is_connected(void);
                    bool wifi_connected = wifi_service_is_connected();
                    
                    if (wifi_connected) {
                        // Stream music from URL
                        audio_set_mode(AUDIO_MODE_HTTP_STREAM);
                        // Example MP3 stream URL - replace with your own!
                        audio_set_url("http://stream.radioparadise.com/aac-320");
                        ESP_LOGI(TAG, "Playing HTTP stream (WiFi connected)");
                        show_notification("Streaming music...", 2000);
                    } else {
                        // Play test tone
                        audio_set_mode(AUDIO_MODE_TEST_TONE);
                        ESP_LOGI(TAG, "Playing test tone (WiFi not connected)");
                        show_notification("Playing test tone", 2000);
                    }
                    
                    // Actually call the audio service!
                    esp_err_t ret = audio_play();
                    if (ret != ESP_OK) {
                        ESP_LOGE(TAG, "Failed to start audio: %s", esp_err_to_name(ret));
                        g_audio.is_playing = false;
                        show_notification("Audio start failed!", 2000);
                        return;
                    }
                    lv_label_set_text(lv_obj_get_child(g_audio.play_pause_button, 0), LV_SYMBOL_PAUSE " Pause");
                    lv_label_set_text(g_audio.status_label, wifi_connected ? "Status: Streaming" : "Status: Playing");
                    ESP_LOGI(TAG, "Audio playback started");
                } else {
                    // Actually stop the audio service!
                    audio_stop();
                    lv_label_set_text(lv_obj_get_child(g_audio.play_pause_button, 0), LV_SYMBOL_PLAY " Play");
                    lv_label_set_text(g_audio.status_label, "Status: Paused");
                    show_notification("Playback paused", 2000);
                    ESP_LOGI(TAG, "Audio playback paused");
                }
            }
            break;
            
        default:
            break;
    }
}
