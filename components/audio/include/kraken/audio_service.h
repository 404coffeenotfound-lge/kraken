#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Audio playback modes
typedef enum {
    AUDIO_MODE_TEST_TONE,    // Generate test tone
    AUDIO_MODE_HTTP_STREAM,  // Stream from HTTP URL
} audio_mode_t;

// Initialize I2S audio with MAX98357A
esp_err_t audio_service_init(void);
esp_err_t audio_service_deinit(void);

// Volume control (0-100)
esp_err_t audio_set_volume(uint8_t volume);
uint8_t audio_get_volume(void);

// Playback control
esp_err_t audio_play(void);
esp_err_t audio_pause(void);
esp_err_t audio_stop(void);
bool audio_is_playing(void);

// Set playback mode and URL
esp_err_t audio_set_mode(audio_mode_t mode);
esp_err_t audio_set_url(const char *url);

// Write audio data
esp_err_t audio_write(const uint8_t *data, size_t len);

#ifdef __cplusplus
}
#endif
