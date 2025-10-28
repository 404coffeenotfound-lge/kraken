#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2s_std.h"
#include "driver/gpio.h"
#include "driver/spi_common.h"

typedef struct {
    spi_host_device_t host;
    int dma_channel;
    int pin_mosi;
    int pin_sclk;
    int pin_cs;
    int pin_dc;
    int pin_rst;
    int pin_bl;
    uint16_t hor_res;
    uint16_t ver_res;
} board_display_config_t;

typedef struct {
    i2s_port_t port;
    int pin_bclk;
    int pin_lrclk;
    int pin_dout;
    int pin_din;
} board_audio_config_t;

typedef struct {
    gpio_num_t pin_up;
    gpio_num_t pin_down;
    gpio_num_t pin_left;
    gpio_num_t pin_right;
    gpio_num_t pin_center;
    bool active_low;
} board_input_config_t;

esp_err_t board_support_init(void);
const board_display_config_t *board_support_get_display_config(void);
const board_audio_config_t *board_support_get_audio_config(void);
const board_input_config_t *board_support_get_input_config(void);
