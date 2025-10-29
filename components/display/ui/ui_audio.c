#include "ui_internal.h"
#include "kraken/wifi_service.h"
#include "kraken/bt_service.h"
#include "kraken/bt_profiles.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include <string.h>

static const char *TAG = "ui_audio";

#define TOPBAR_HEIGHT 30

typedef enum {
    AUDIO_SCREEN_MAIN = 0,
    AUDIO_SCREEN_BT_PLAYBACK,
} audio_screen_state_t;

typedef enum {
    FOCUS_BACK_BUTTON = 0,
    FOCUS_BT_PLAYBACK_MENU,
} audio_focus_t;

typedef enum {
    BT_FOCUS_BACK = 0,
    BT_FOCUS_DEVICE_LIST,
    BT_FOCUS_PLAY_BUTTON,
} bt_playback_focus_t;

static struct {
    lv_obj_t *screen;
    lv_obj_t *back_button;
    lv_obj_t *bt_playback_button;
    lv_obj_t *notification;
    
    lv_obj_t *bt_screen;
    lv_obj_t *bt_back_button;
    lv_obj_t *bt_device_list;
    lv_obj_t *bt_play_button;
    lv_obj_t *bt_status_label;
    
    audio_screen_state_t state;
    audio_focus_t focus;
    bt_playback_focus_t bt_focus;
    
    bt_scan_result_t scan_results;
    int selected_device_index;
    uint8_t selected_mac[BT_MAC_ADDR_LEN];
    bool is_playing;
} g_audio = {0};

static void show_main_screen(void);
static void show_bt_playback_screen(void);
static void notification_timer_cb(lv_timer_t *timer);

static void show_notification(const char *message, uint32_t duration_ms)
{
    if (g_audio.notification) {
        lv_obj_del(g_audio.notification);
    }

    g_audio.notification = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_audio.notification, LV_PCT(80), 60);
    lv_obj_align(g_audio.notification, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_audio.notification, lv_color_hex(0xE0E0E0), 0);
    lv_obj_set_style_radius(g_audio.notification, 0, 0);
    lv_obj_set_style_border_color(g_audio.notification, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(g_audio.notification, 2, 0);

    lv_obj_t *label = lv_label_create(g_audio.notification);
    lv_label_set_text(label, message);
    lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
    lv_obj_center(label);

    if (duration_ms > 0) {
        lv_timer_t *timer = lv_timer_create(notification_timer_cb, duration_ms, NULL);
        lv_timer_set_repeat_count(timer, 1);
    }

    ESP_LOGI(TAG, "Notification: %s", message);
}

static void notification_timer_cb(lv_timer_t *timer)
{
    if (g_audio.notification) {
        lv_obj_del(g_audio.notification);
        g_audio.notification = NULL;
    }
}

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            ESP_LOGI(TAG, "Received %d bytes", evt->data_len);
            if (g_audio.is_playing) {
                bt_a2dp_source_write_data((uint8_t *)evt->data, evt->data_len);
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "Audio download finished");
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP Error");
            break;
        default:
            break;
    }
    return ESP_OK;
}

static void download_and_play_audio(void)
{
    if (!wifi_service_is_connected()) {
        show_notification("WiFi not connected!", 3000);
        return;
    }

    if (g_audio.selected_device_index < 0) {
        show_notification("No device selected!", 3000);
        return;
    }

    if (!bt_profile_is_enabled(BT_PROFILE_A2DP_SOURCE)) {
        bt_profile_enable(BT_PROFILE_A2DP_SOURCE);
        vTaskDelay(pdMS_TO_TICKS(500));
    }

    ESP_LOGI(TAG, "Connecting to Bluetooth device...");
    show_notification("Connecting...", 2000);
    
    esp_err_t ret = bt_a2dp_source_connect(g_audio.selected_mac);
    if (ret != ESP_OK) {
        show_notification("Connect failed!", 3000);
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(3000));
    
    ret = bt_a2dp_source_start_stream();
    if (ret != ESP_OK) {
        show_notification("Stream failed!", 3000);
        return;
    }

    g_audio.is_playing = true;
    
    show_notification("Playing...", 2000);
    ESP_LOGI(TAG, "Starting audio playback");

    esp_http_client_config_t config = {
        .url = "http://www.soundhelix.com/examples/mp3/SoundHelix-Song-1.mp3",
        .event_handler = http_event_handler,
        .buffer_size = 4096,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);
    
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Playback completed");
        show_notification("Finished!", 3000);
    } else {
        ESP_LOGE(TAG, "Playback failed");
        show_notification("Failed!", 3000);
    }
    
    esp_http_client_cleanup(client);
    bt_a2dp_source_stop_stream();
    vTaskDelay(pdMS_TO_TICKS(500));
    bt_a2dp_source_disconnect();
    
    g_audio.is_playing = false;
    
    if (g_audio.bt_status_label) {
        lv_label_set_text(g_audio.bt_status_label, "Ready");
    }
}

static void scan_bluetooth_devices(void)
{
    if (!bt_service_is_enabled()) {
        show_notification("BT not enabled!", 3000);
        return;
    }

    show_notification("Scanning...", 2000);
    bt_service_scan(10);
}

static void update_device_list(void)
{
    if (!g_audio.bt_device_list) {
        return;
    }

    lv_obj_clean(g_audio.bt_device_list);
    bt_service_get_scan_results(&g_audio.scan_results);
    
    if (g_audio.scan_results.count == 0) {
        lv_obj_t *label = lv_label_create(g_audio.bt_device_list);
        lv_label_set_text(label, "No devices found");
        lv_obj_set_style_text_color(label, lv_color_hex(0x888888), 0);
        return;
    }

    for (int i = 0; i < g_audio.scan_results.count; i++) {
        lv_obj_t *item = lv_obj_create(g_audio.bt_device_list);
        lv_obj_set_width(item, lv_pct(100));
        lv_obj_set_height(item, 40);
        
        if (i == g_audio.selected_device_index) {
            lv_obj_set_style_bg_color(item, lv_color_hex(0x00FF00), 0);
            lv_obj_set_style_border_width(item, 2, 0);
        } else {
            lv_obj_set_style_bg_color(item, lv_color_hex(0xFFFFFF), 0);
            lv_obj_set_style_border_width(item, 1, 0);
        }
        
        lv_obj_set_style_radius(item, 0, 0);
        lv_obj_set_style_pad_all(item, 5, 0);

        lv_obj_t *label = lv_label_create(item);
        char device_info[128];
        snprintf(device_info, sizeof(device_info), "%s (%ddBm)",
                 g_audio.scan_results.devices[i].name[0] ? 
                 g_audio.scan_results.devices[i].name : "Unknown",
                 g_audio.scan_results.devices[i].rssi);
        lv_label_set_text(label, device_info);
        lv_obj_set_style_text_color(label, lv_color_hex(0x000000), 0);
        lv_obj_align(label, LV_ALIGN_LEFT_MID, 0, 0);
    }
}

static void show_bt_playback_screen(void)
{
    if (g_audio.bt_screen) {
        lv_obj_del(g_audio.bt_screen);
    }

    g_audio.state = AUDIO_SCREEN_BT_PLAYBACK;
    g_audio.bt_focus = BT_FOCUS_BACK;
    g_audio.selected_device_index = -1;

    g_audio.bt_screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_audio.bt_screen, LV_HOR_RES, LV_VER_RES - TOPBAR_HEIGHT);
    lv_obj_set_pos(g_audio.bt_screen, 0, TOPBAR_HEIGHT);
    lv_obj_set_style_bg_color(g_audio.bt_screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_audio.bt_screen, 0, 0);
    lv_obj_set_style_radius(g_audio.bt_screen, 0, 0);
    lv_obj_set_style_pad_all(g_audio.bt_screen, 10, 0);
    lv_obj_clear_flag(g_audio.bt_screen, LV_OBJ_FLAG_SCROLLABLE);

    g_audio.bt_back_button = lv_obj_create(g_audio.bt_screen);
    lv_obj_set_size(g_audio.bt_back_button, 40, 30);
    lv_obj_align(g_audio.bt_back_button, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_audio.bt_back_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_audio.bt_back_button, 0, 0);
    lv_obj_set_style_border_width(g_audio.bt_back_button, 1, 0);

    lv_obj_t *back_label = lv_label_create(g_audio.bt_back_button);
    lv_label_set_text(back_label, "<");
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(g_audio.bt_screen);
    lv_label_set_text(title, "BT Audio Playback");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    g_audio.bt_status_label = lv_label_create(g_audio.bt_screen);
    lv_label_set_text(g_audio.bt_status_label, "Ready");
    lv_obj_align(g_audio.bt_status_label, LV_ALIGN_TOP_MID, 0, 30);
    lv_obj_set_style_text_color(g_audio.bt_status_label, lv_color_hex(0x0000FF), 0);

    g_audio.bt_device_list = lv_obj_create(g_audio.bt_screen);
    lv_obj_set_size(g_audio.bt_device_list, LV_PCT(100), 120);
    lv_obj_align(g_audio.bt_device_list, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_set_style_bg_color(g_audio.bt_device_list, lv_color_hex(0xF0F0F0), 0);
    lv_obj_set_style_radius(g_audio.bt_device_list, 0, 0);
    lv_obj_set_flex_flow(g_audio.bt_device_list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_audio.bt_device_list, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(g_audio.bt_device_list, LV_SCROLLBAR_MODE_AUTO);

    g_audio.bt_play_button = lv_obj_create(g_audio.bt_screen);
    lv_obj_set_size(g_audio.bt_play_button, LV_PCT(80), 40);
    lv_obj_align(g_audio.bt_play_button, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_bg_color(g_audio.bt_play_button, lv_color_hex(0x00AA00), 0);
    lv_obj_set_style_radius(g_audio.bt_play_button, 0, 0);

    lv_obj_t *play_label = lv_label_create(g_audio.bt_play_button);
    lv_label_set_text(play_label, "Play from Internet");
    lv_obj_set_style_text_color(play_label, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(play_label);

    scan_bluetooth_devices();
    vTaskDelay(pdMS_TO_TICKS(1000));
    update_device_list();
}

static void show_main_screen(void)
{
    if (g_audio.screen) {
        lv_obj_del(g_audio.screen);
    }

    g_audio.state = AUDIO_SCREEN_MAIN;
    g_audio.focus = FOCUS_BACK_BUTTON;

    g_audio.screen = lv_obj_create(lv_screen_active());
    lv_obj_set_size(g_audio.screen, LV_HOR_RES, LV_VER_RES - TOPBAR_HEIGHT);
    lv_obj_set_pos(g_audio.screen, 0, TOPBAR_HEIGHT);
    lv_obj_set_style_bg_color(g_audio.screen, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_border_width(g_audio.screen, 0, 0);
    lv_obj_set_style_radius(g_audio.screen, 0, 0);
    lv_obj_set_style_pad_all(g_audio.screen, 10, 0);
    lv_obj_clear_flag(g_audio.screen, LV_OBJ_FLAG_SCROLLABLE);

    g_audio.back_button = lv_obj_create(g_audio.screen);
    lv_obj_set_size(g_audio.back_button, 40, 30);
    lv_obj_align(g_audio.back_button, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(g_audio.back_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_audio.back_button, 0, 0);
    lv_obj_set_style_border_width(g_audio.back_button, 1, 0);

    lv_obj_t *back_label = lv_label_create(g_audio.back_button);
    lv_label_set_text(back_label, "<");
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(g_audio.screen);
    lv_label_set_text(title, "Audio Menu");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 5);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_14, 0);

    g_audio.bt_playback_button = lv_obj_create(g_audio.screen);
    lv_obj_set_size(g_audio.bt_playback_button, LV_PCT(90), 50);
    lv_obj_align(g_audio.bt_playback_button, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(g_audio.bt_playback_button, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_radius(g_audio.bt_playback_button, 0, 0);
    lv_obj_set_style_border_width(g_audio.bt_playback_button, 2, 0);

    lv_obj_t *bt_label = lv_label_create(g_audio.bt_playback_button);
    lv_label_set_text(bt_label, "Bluetooth Playback");
    lv_obj_set_style_text_font(bt_label, &lv_font_montserrat_14, 0);
    lv_obj_center(bt_label);

    lv_obj_set_style_border_color(g_audio.back_button, lv_color_hex(0x00FF00), 0);
    lv_obj_set_style_border_width(g_audio.back_button, 2, 0);
}

void ui_audio_handle_input(kraken_event_type_t input)
{
    if (g_audio.state == AUDIO_SCREEN_MAIN) {
        if (input == KRAKEN_EVENT_INPUT_UP) {
            g_audio.focus = (g_audio.focus > 0) ? g_audio.focus - 1 : FOCUS_BT_PLAYBACK_MENU;
        } else if (input == KRAKEN_EVENT_INPUT_DOWN) {
            g_audio.focus = (g_audio.focus < FOCUS_BT_PLAYBACK_MENU) ? g_audio.focus + 1 : FOCUS_BACK_BUTTON;
        } else if (input == KRAKEN_EVENT_INPUT_CENTER) {
            if (g_audio.focus == FOCUS_BACK_BUTTON) {
                ui_manager_exit_submenu();
                return;
            } else if (g_audio.focus == FOCUS_BT_PLAYBACK_MENU) {
                show_bt_playback_screen();
                return;
            }
        }

        lv_obj_set_style_border_color(g_audio.back_button, 
            g_audio.focus == FOCUS_BACK_BUTTON ? lv_color_hex(0x00FF00) : lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(g_audio.back_button, 
            g_audio.focus == FOCUS_BACK_BUTTON ? 2 : 1, 0);
        
        lv_obj_set_style_border_color(g_audio.bt_playback_button, 
            g_audio.focus == FOCUS_BT_PLAYBACK_MENU ? lv_color_hex(0x00FF00) : lv_color_hex(0x000000), 0);
    } 
    else if (g_audio.state == AUDIO_SCREEN_BT_PLAYBACK) {
        if (input == KRAKEN_EVENT_INPUT_UP) {
            if (g_audio.bt_focus == BT_FOCUS_DEVICE_LIST && g_audio.selected_device_index > 0) {
                g_audio.selected_device_index--;
                update_device_list();
            } else if (g_audio.bt_focus > BT_FOCUS_BACK) {
                g_audio.bt_focus--;
            }
        } else if (input == KRAKEN_EVENT_INPUT_DOWN) {
            if (g_audio.bt_focus == BT_FOCUS_DEVICE_LIST && 
                g_audio.selected_device_index < g_audio.scan_results.count - 1) {
                g_audio.selected_device_index++;
                update_device_list();
            } else if (g_audio.bt_focus < BT_FOCUS_PLAY_BUTTON) {
                g_audio.bt_focus++;
            }
        } else if (input == KRAKEN_EVENT_INPUT_CENTER) {
            if (g_audio.bt_focus == BT_FOCUS_BACK) {
                lv_obj_del(g_audio.bt_screen);
                g_audio.bt_screen = NULL;
                show_main_screen();
                return;
            } else if (g_audio.bt_focus == BT_FOCUS_DEVICE_LIST && g_audio.selected_device_index >= 0) {
                memcpy(g_audio.selected_mac, 
                       g_audio.scan_results.devices[g_audio.selected_device_index].mac,
                       BT_MAC_ADDR_LEN);
                show_notification("Device selected!", 1000);
            } else if (g_audio.bt_focus == BT_FOCUS_PLAY_BUTTON) {
                if (g_audio.bt_status_label) {
                    lv_label_set_text(g_audio.bt_status_label, "Playing...");
                }
                download_and_play_audio();
            }
        }

        lv_obj_set_style_border_color(g_audio.bt_back_button, 
            g_audio.bt_focus == BT_FOCUS_BACK ? lv_color_hex(0x00FF00) : lv_color_hex(0x000000), 0);
        lv_obj_set_style_border_width(g_audio.bt_back_button, 
            g_audio.bt_focus == BT_FOCUS_BACK ? 2 : 1, 0);
            
        lv_obj_set_style_border_color(g_audio.bt_play_button, 
            g_audio.bt_focus == BT_FOCUS_PLAY_BUTTON ? lv_color_hex(0x00FF00) : lv_color_hex(0x00AA00), 0);
        lv_obj_set_style_border_width(g_audio.bt_play_button, 
            g_audio.bt_focus == BT_FOCUS_PLAY_BUTTON ? 3 : 0, 0);
    }
}

lv_obj_t *ui_audio_screen_create(lv_obj_t *parent)
{
    memset(&g_audio, 0, sizeof(g_audio));
    g_audio.selected_device_index = -1;
    show_main_screen();
    return g_audio.screen;
}

void ui_audio_screen_show(void)
{
    if (g_audio.screen) {
        lv_obj_clear_flag(g_audio.screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_audio_screen_hide(void)
{
    if (g_audio.screen) {
        lv_obj_add_flag(g_audio.screen, LV_OBJ_FLAG_HIDDEN);
    }
}

void ui_audio_screen_delete(void)
{
    if (g_audio.notification) {
        lv_obj_del(g_audio.notification);
        g_audio.notification = NULL;
    }
    
    if (g_audio.bt_screen) {
        lv_obj_del(g_audio.bt_screen);
        g_audio.bt_screen = NULL;
    }
    
    if (g_audio.screen) {
        lv_obj_del(g_audio.screen);
        g_audio.screen = NULL;
    }
    
    memset(&g_audio, 0, sizeof(g_audio));
}
