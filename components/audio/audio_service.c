#include "kraken/audio_service.h"
#include "kraken/bsp.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <math.h>

static const char *TAG = "audio_service";

// I2S Configuration
#define I2S_SAMPLE_RATE 44100
#define I2S_BITS_PER_SAMPLE 16
#define TEST_TONE_FREQUENCY 440  // A4 note (440 Hz)
#define HTTP_BUFFER_SIZE 4096

static struct {
    bool initialized;
    bool is_playing;
    uint8_t volume;
    audio_mode_t mode;
    char url[256];
    i2s_chan_handle_t tx_handle;
    const board_audio_config_t *config;
    TaskHandle_t audio_task;
    esp_http_client_handle_t http_client;
} g_audio = {0};

// HTTP streaming task
static void http_stream_audio(void)
{
    esp_http_client_config_t config = {
        .url = g_audio.url,
        .timeout_ms = 5000,
        .buffer_size = HTTP_BUFFER_SIZE,
    };
    
    g_audio.http_client = esp_http_client_init(&config);
    if (!g_audio.http_client) {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        return;
    }
    
    esp_err_t err = esp_http_client_open(g_audio.http_client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(err));
        esp_http_client_cleanup(g_audio.http_client);
        g_audio.http_client = NULL;
        return;
    }
    
    int content_length = esp_http_client_fetch_headers(g_audio.http_client);
    ESP_LOGI(TAG, "HTTP stream opened, content_length=%d", content_length);
    
    uint8_t *buffer = malloc(HTTP_BUFFER_SIZE);
    if (!buffer) {
        ESP_LOGE(TAG, "Failed to allocate HTTP buffer");
        esp_http_client_close(g_audio.http_client);
        esp_http_client_cleanup(g_audio.http_client);
        g_audio.http_client = NULL;
        return;
    }
    
    int total_bytes = 0;
    while (g_audio.is_playing) {
        int read_len = esp_http_client_read(g_audio.http_client, (char *)buffer, HTTP_BUFFER_SIZE);
        if (read_len <= 0) {
            ESP_LOGW(TAG, "HTTP stream ended or error");
            break;
        }
        
        // Apply volume scaling to streamed data (assume 16-bit samples)
        if (g_audio.volume == 0) {
            // Mute: write silence instead
            memset(buffer, 0, read_len);
        } else if (g_audio.volume < 100) {
            // Scale volume (use quadratic curve for better control)
            float volume_scale = powf(g_audio.volume / 100.0f, 2.0f);
            int16_t *samples = (int16_t *)buffer;
            int num_samples = read_len / sizeof(int16_t);
            for (int i = 0; i < num_samples; i++) {
                samples[i] = (int16_t)(samples[i] * volume_scale);
            }
        }
        
        // Write to I2S
        size_t bytes_written = 0;
        esp_err_t ret = i2s_channel_write(g_audio.tx_handle, buffer, read_len, &bytes_written, portMAX_DELAY);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
            break;
        }
        
        total_bytes += bytes_written;
        if (total_bytes % (HTTP_BUFFER_SIZE * 10) == 0) {
            ESP_LOGI(TAG, "Streamed %d bytes, volume=%d%%", total_bytes, g_audio.volume);
        }
    }
    
    free(buffer);
    esp_http_client_close(g_audio.http_client);
    esp_http_client_cleanup(g_audio.http_client);
    g_audio.http_client = NULL;
    ESP_LOGI(TAG, "HTTP streaming stopped, total bytes: %d", total_bytes);
}

// Audio playback task
static void audio_task(void *arg)
{
    const int buffer_size = 1024;
    int16_t audio_buffer[buffer_size];
    float phase = 0.0f;
    const float phase_increment = (2.0f * M_PI * TEST_TONE_FREQUENCY) / I2S_SAMPLE_RATE;
    int buffer_count = 0;
    
    ESP_LOGI(TAG, "Audio playback task started");
    
    while (1) {
        if (g_audio.is_playing) {
            if (g_audio.mode == AUDIO_MODE_HTTP_STREAM) {
                ESP_LOGI(TAG, "Starting HTTP stream from: %s", g_audio.url);
                http_stream_audio();
                g_audio.is_playing = false;  // Stop after stream ends
                ESP_LOGI(TAG, "HTTP stream finished");
                continue;
            }
            
            // Test tone mode
            if (buffer_count == 0) {
                ESP_LOGI(TAG, "Generating %d Hz test tone at sample rate %d, volume %d%%", 
                         TEST_TONE_FREQUENCY, I2S_SAMPLE_RATE, g_audio.volume);
            }
            
            // Generate sine wave test tone with volume control
            // If volume is 0, skip generation and write silence
            if (g_audio.volume == 0) {
                memset(audio_buffer, 0, buffer_size * sizeof(int16_t));
            } else {
                // Use logarithmic volume curve for better perceived control
                // Human hearing is logarithmic, so linear scaling sounds bad
                float volume_scale = powf(g_audio.volume / 100.0f, 2.0f);  // Quadratic curve
                
                for (int i = 0; i < buffer_size / 2; i++) {  // Stereo, so /2 samples
                    // Use 80% of full scale (26214 out of 32767) to avoid clipping
                    int16_t sample = (int16_t)(sin(phase) * 26214.0f * volume_scale);
                    audio_buffer[i * 2] = sample;      // Left channel
                    audio_buffer[i * 2 + 1] = sample;  // Right channel
                    phase += phase_increment;
                    if (phase >= 2.0f * M_PI) {
                        phase -= 2.0f * M_PI;
                    }
                }
            }
            
            // Write to I2S
            size_t bytes_written = 0;
            esp_err_t ret = i2s_channel_write(g_audio.tx_handle, audio_buffer, 
                                              buffer_size * sizeof(int16_t), 
                                              &bytes_written, portMAX_DELAY);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
                buffer_count = 0;  // Reset on error
            } else {
                buffer_count++;
                if (buffer_count == 1) {
                    ESP_LOGI(TAG, "First I2S buffer written successfully (%d bytes)", bytes_written);
                }
                if (buffer_count % 100 == 0) {  // Log every 100 buffers (~2 seconds)
                    ESP_LOGI(TAG, "Audio playing: %d buffers written, volume=%d%%, bytes_per_buffer=%d", 
                             buffer_count, g_audio.volume, bytes_written);
                }
            }
        } else {
            // Not playing, just delay
            vTaskDelay(pdMS_TO_TICKS(100));
            phase = 0.0f;  // Reset phase when stopped
            if (buffer_count > 0) {
                ESP_LOGI(TAG, "Playback stopped. Total buffers written: %d", buffer_count);
                buffer_count = 0;
            }
        }
    }
}

esp_err_t audio_service_init(void)
{
    if (g_audio.initialized) {
        return ESP_OK;
    }

    esp_err_t ret;
    
    // Get BSP audio configuration
    g_audio.config = board_support_get_audio_config();
    if (!g_audio.config) {
        ESP_LOGE(TAG, "Failed to get BSP audio config");
        return ESP_ERR_INVALID_STATE;
    }

    // Configure shutdown pin (SD) - HIGH to enable, LOW to disable
    if (g_audio.config->pin_sd >= 0) {
        gpio_config_t io_conf = {
            .pin_bit_mask = (1ULL << g_audio.config->pin_sd),
            .mode = GPIO_MODE_OUTPUT,
            .pull_up_en = GPIO_PULLUP_DISABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type = GPIO_INTR_DISABLE,
        };
        gpio_config(&io_conf);
        gpio_set_level(g_audio.config->pin_sd, 1);  // Enable MAX98357A
    }

    // Configure I2S channel
    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(g_audio.config->port, I2S_ROLE_MASTER);
    ret = i2s_new_channel(&chan_cfg, &g_audio.tx_handle, NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create I2S channel: %s", esp_err_to_name(ret));
        return ret;
    }

    // Configure I2S standard mode (for MAX98357A)
    // MAX98357A works with I2S Philips mode (standard I2S)
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,  // MAX98357A doesn't need MCLK
            .bclk = g_audio.config->pin_bclk,
            .ws = g_audio.config->pin_lrclk,
            .dout = g_audio.config->pin_dout,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    
    ESP_LOGI(TAG, "Configuring I2S: Sample Rate=%d, 16-bit Stereo, Philips mode", I2S_SAMPLE_RATE);

    ret = i2s_channel_init_std_mode(g_audio.tx_handle, &std_cfg);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(ret));
        i2s_del_channel(g_audio.tx_handle);
        return ret;
    }

    // Enable the I2S channel
    ret = i2s_channel_enable(g_audio.tx_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(ret));
        i2s_del_channel(g_audio.tx_handle);
        return ret;
    }
    
    // Preload I2S with silence to start clock
    int16_t silence[64] = {0};
    size_t bytes_written = 0;
    i2s_channel_write(g_audio.tx_handle, silence, sizeof(silence), &bytes_written, 100);
    ESP_LOGI(TAG, "I2S preloaded with %d bytes of silence", bytes_written);

    g_audio.volume = 50;  // Default 50% volume
    g_audio.is_playing = false;
    g_audio.mode = AUDIO_MODE_TEST_TONE;  // Default mode
    g_audio.url[0] = '\0';  // Empty URL initially
    g_audio.http_client = NULL;
    
    // IMPORTANT: Set initialized flag BEFORE creating task!
    g_audio.initialized = true;

    // Create audio playback task
    BaseType_t task_ret = xTaskCreate(audio_task, "audio_task", 8192, NULL, 5, &g_audio.audio_task);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create audio task");
        g_audio.initialized = false;
        i2s_channel_disable(g_audio.tx_handle);
        i2s_del_channel(g_audio.tx_handle);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "MAX98357A I2S audio initialized (from BSP config)");
    ESP_LOGI(TAG, "I2S Pins - BCLK:%d, WS/LRC:%d, DOUT/DIN:%d, SD:%d", 
             g_audio.config->pin_bclk, g_audio.config->pin_lrclk, 
             g_audio.config->pin_dout, g_audio.config->pin_sd);
    ESP_LOGI(TAG, "Test tone: %d Hz", TEST_TONE_FREQUENCY);
    
    return ESP_OK;
}

esp_err_t audio_service_deinit(void)
{
    if (!g_audio.initialized) {
        return ESP_OK;
    }

    audio_stop();
    
    // Delete audio task
    if (g_audio.audio_task) {
        vTaskDelete(g_audio.audio_task);
        g_audio.audio_task = NULL;
    }
    
    if (g_audio.tx_handle) {
        i2s_channel_disable(g_audio.tx_handle);
        i2s_del_channel(g_audio.tx_handle);
        g_audio.tx_handle = NULL;
    }

    // Shutdown MAX98357A
    if (g_audio.config && g_audio.config->pin_sd >= 0) {
        gpio_set_level(g_audio.config->pin_sd, 0);
    }

    g_audio.initialized = false;
    ESP_LOGI(TAG, "Audio service deinitialized");
    
    return ESP_OK;
}

esp_err_t audio_set_volume(uint8_t volume)
{
    if (volume > 100) {
        volume = 100;
    }
    
    g_audio.volume = volume;
    
    // MAX98357A volume control via SD (shutdown) pin
    // SD pin LOW = mute, SD pin HIGH = unmute
    // When volume is 0, we mute the amplifier
    if (g_audio.config && g_audio.config->pin_sd >= 0) {
        if (volume == 0) {
            gpio_set_level(g_audio.config->pin_sd, 0);  // Mute
            ESP_LOGI(TAG, "Volume set to 0%% (MUTED via SD pin)");
        } else {
            gpio_set_level(g_audio.config->pin_sd, 1);  // Unmute
            ESP_LOGI(TAG, "Volume set to %d%% (software scaling)", volume);
        }
    } else {
        ESP_LOGI(TAG, "Volume set to %d%% (software scaling only)", volume);
    }
    
    return ESP_OK;
}

uint8_t audio_get_volume(void)
{
    return g_audio.volume;
}

esp_err_t audio_play(void)
{
    if (!g_audio.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Enable MAX98357A
    if (g_audio.config && g_audio.config->pin_sd >= 0) {
        gpio_set_level(g_audio.config->pin_sd, 1);
        ESP_LOGI(TAG, "SD pin (GPIO %d) set HIGH", g_audio.config->pin_sd);
    }
    g_audio.is_playing = true;
    
    ESP_LOGI(TAG, "Audio playback started (volume=%d%%)", g_audio.volume);
    return ESP_OK;
}

esp_err_t audio_pause(void)
{
    if (!g_audio.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Mute MAX98357A (SD pin LOW)
    if (g_audio.config && g_audio.config->pin_sd >= 0) {
        gpio_set_level(g_audio.config->pin_sd, 0);
    }
    g_audio.is_playing = false;
    
    ESP_LOGI(TAG, "Audio playback paused");
    return ESP_OK;
}

esp_err_t audio_stop(void)
{
    if (!g_audio.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Mute MAX98357A
    if (g_audio.config && g_audio.config->pin_sd >= 0) {
        gpio_set_level(g_audio.config->pin_sd, 0);
    }
    g_audio.is_playing = false;
    
    ESP_LOGI(TAG, "Audio playback stopped");
    return ESP_OK;
}

bool audio_is_playing(void)
{
    return g_audio.is_playing;
}

esp_err_t audio_set_mode(audio_mode_t mode)
{
    if (!g_audio.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    g_audio.mode = mode;
    ESP_LOGI(TAG, "Audio mode set to: %s", 
             mode == AUDIO_MODE_TEST_TONE ? "TEST_TONE" : "HTTP_STREAM");
    return ESP_OK;
}

esp_err_t audio_set_url(const char *url)
{
    if (!g_audio.initialized || !url) {
        return ESP_ERR_INVALID_ARG;
    }
    
    strncpy(g_audio.url, url, sizeof(g_audio.url) - 1);
    g_audio.url[sizeof(g_audio.url) - 1] = '\0';
    
    ESP_LOGI(TAG, "Audio URL set to: %s", g_audio.url);
    return ESP_OK;
}

esp_err_t audio_write(const uint8_t *data, size_t len)
{
    if (!g_audio.initialized || !data || len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!g_audio.is_playing) {
        return ESP_ERR_INVALID_STATE;
    }

    size_t bytes_written = 0;
    esp_err_t ret = i2s_channel_write(g_audio.tx_handle, data, len, &bytes_written, portMAX_DELAY);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(ret));
        return ret;
    }

    return ESP_OK;
}
