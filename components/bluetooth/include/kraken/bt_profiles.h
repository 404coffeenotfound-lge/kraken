#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Bluetooth profile types
typedef enum {
    BT_PROFILE_NONE = 0,
    BT_PROFILE_SPP,        // Serial Port Profile
    BT_PROFILE_A2DP_SINK,  // Audio Sink (receive audio)
    BT_PROFILE_A2DP_SOURCE, // Audio Source (send audio)
    BT_PROFILE_AVRCP,      // Audio/Video Remote Control
    BT_PROFILE_HFP_CLIENT, // Hands-Free Profile (as client)
    BT_PROFILE_HID_HOST,   // HID Host (receive input from devices)
    BT_PROFILE_HID_DEVICE, // HID Device (act as keyboard/mouse)
} bt_profile_t;

// Profile status callback
typedef void (*bt_profile_status_cb_t)(bt_profile_t profile, bool connected, void *user_data);

// SPP data callback
typedef void (*bt_spp_data_cb_t)(const uint8_t *data, uint16_t len, void *user_data);

// A2DP audio data callback
typedef void (*bt_a2dp_data_cb_t)(const uint8_t *data, uint32_t len, void *user_data);

// HID data callback
typedef void (*bt_hid_data_cb_t)(uint8_t report_type, const uint8_t *data, uint16_t len, void *user_data);

/**
 * @brief Initialize Bluetooth profiles
 */
esp_err_t bt_profiles_init(void);

/**
 * @brief Deinitialize Bluetooth profiles
 */
esp_err_t bt_profiles_deinit(void);

/**
 * @brief Enable specific profile
 * @param profile Profile to enable
 */
esp_err_t bt_profile_enable(bt_profile_t profile);

/**
 * @brief Disable specific profile
 * @param profile Profile to disable
 */
esp_err_t bt_profile_disable(bt_profile_t profile);

/**
 * @brief Check if profile is enabled
 */
bool bt_profile_is_enabled(bt_profile_t profile);

/**
 * @brief Set profile status callback
 */
void bt_profile_set_status_callback(bt_profile_status_cb_t cb, void *user_data);

// SPP Functions
esp_err_t bt_spp_write(const uint8_t *data, uint16_t len);
void bt_spp_set_data_callback(bt_spp_data_cb_t cb, void *user_data);

// A2DP Functions
esp_err_t bt_a2dp_start_playback(void);
esp_err_t bt_a2dp_stop_playback(void);
void bt_a2dp_set_data_callback(bt_a2dp_data_cb_t cb, void *user_data);

// A2DP Source Functions (send audio to headphones/speakers)
esp_err_t bt_a2dp_source_connect(const uint8_t *remote_bda);
esp_err_t bt_a2dp_source_disconnect(void);
esp_err_t bt_a2dp_source_start_stream(void);
esp_err_t bt_a2dp_source_stop_stream(void);
esp_err_t bt_a2dp_source_write_data(const uint8_t *data, uint32_t len);

// AVRCP Functions
esp_err_t bt_avrcp_send_play(void);
esp_err_t bt_avrcp_send_pause(void);
esp_err_t bt_avrcp_send_next(void);
esp_err_t bt_avrcp_send_prev(void);
esp_err_t bt_avrcp_set_volume(uint8_t volume); // 0-127

// HID Functions
esp_err_t bt_hid_send_keyboard(uint8_t modifier, uint8_t *keycodes, uint8_t keycode_count);
esp_err_t bt_hid_send_mouse(int8_t x, int8_t y, uint8_t buttons);
void bt_hid_set_data_callback(bt_hid_data_cb_t cb, void *user_data);

#ifdef __cplusplus
}
#endif
