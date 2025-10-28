#include "kraken/bsp.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "board";

static const board_display_config_t s_display_config = {
    .host = SPI2_HOST,
    .dma_channel = SPI_DMA_CH_AUTO,
    .pin_mosi = 17,
    .pin_sclk = 18,
    .pin_cs = 14,
    .pin_dc = 15,
    .pin_rst = 16,
    .pin_bl = 13,
    .hor_res = 240,
    .ver_res = 320,
};

static const board_audio_config_t s_audio_config = {
    .port = I2S_NUM_0,
    .pin_bclk = 4,
    .pin_lrclk = 5,
    .pin_dout = 6,
    .pin_din = 7,
};

static const board_input_config_t s_input_config = {
    .pin_up = GPIO_NUM_21,
    .pin_down = GPIO_NUM_20,
    .pin_left = GPIO_NUM_19,
    .pin_right = GPIO_NUM_8,
    .pin_center = GPIO_NUM_9,
    .active_low = true,
};

esp_err_t board_support_init(void)
{
    gpio_config_t io_conf = {
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_num_t nav_pins[] = {
        s_input_config.pin_up,
        s_input_config.pin_down,
        s_input_config.pin_left,
        s_input_config.pin_right,
        s_input_config.pin_center,
    };
    for (size_t i = 0; i < sizeof(nav_pins) / sizeof(nav_pins[0]); ++i) {
        if (nav_pins[i] == GPIO_NUM_NC) {
            continue;
        }
        io_conf.pin_bit_mask = 1ULL << nav_pins[i];
        ESP_RETURN_ON_ERROR(gpio_config(&io_conf), TAG, "Failed to configure nav pin");
    }
    gpio_config_t bl_conf = {
        .pin_bit_mask = 1ULL << s_display_config.pin_bl,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_RETURN_ON_ERROR(gpio_config(&bl_conf), TAG, "Failed to setup backlight");
    ESP_RETURN_ON_ERROR(gpio_set_level(s_display_config.pin_bl, 1), TAG, "BL on");
    ESP_LOGI(TAG, "Board support initialized");
    return ESP_OK;
}

const board_display_config_t *board_support_get_display_config(void)
{
    return &s_display_config;
}

const board_audio_config_t *board_support_get_audio_config(void)
{
    return &s_audio_config;
}

const board_input_config_t *board_support_get_input_config(void)
{
    return &s_input_config;
}