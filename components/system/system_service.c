#include "kraken/system_service.h"
#include "kraken/kernel.h"
#include "kraken/bsp.h"
#include "esp_sntp.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <time.h>
#include <sys/time.h>

static const char *TAG = "system_service";

static struct {
    bool initialized;
    bool time_synced;
    bool input_monitor_running;
    TaskHandle_t input_task_handle;
    const board_input_config_t *input_cfg;
} g_system = {0};

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Time synchronized");
    g_system.time_synced = true;
    kraken_event_post(KRAKEN_EVENT_SYSTEM_TIME_SYNC, NULL, 0);
}

static void wifi_event_handler(const kraken_event_t *event, void *user_data)
{
    if (event->type == KRAKEN_EVENT_WIFI_GOT_IP) {
        ESP_LOGI(TAG, "WiFi connected, syncing time...");
        system_service_sync_time("pool.ntp.org", "GMT-7");
    }
}

static void input_monitor_task(void *arg)
{
    const board_input_config_t *cfg = g_system.input_cfg;
    uint32_t prev_state = 0;
    
    while (g_system.input_monitor_running) {
        uint32_t curr_state = 0;
        
        if (cfg->pin_up != GPIO_NUM_NC) {
            curr_state |= (gpio_get_level(cfg->pin_up) == (cfg->active_low ? 0 : 1)) << 0;
        }
        if (cfg->pin_down != GPIO_NUM_NC) {
            curr_state |= (gpio_get_level(cfg->pin_down) == (cfg->active_low ? 0 : 1)) << 1;
        }
        if (cfg->pin_left != GPIO_NUM_NC) {
            curr_state |= (gpio_get_level(cfg->pin_left) == (cfg->active_low ? 0 : 1)) << 2;
        }
        if (cfg->pin_right != GPIO_NUM_NC) {
            curr_state |= (gpio_get_level(cfg->pin_right) == (cfg->active_low ? 0 : 1)) << 3;
        }
        if (cfg->pin_center != GPIO_NUM_NC) {
            curr_state |= (gpio_get_level(cfg->pin_center) == (cfg->active_low ? 0 : 1)) << 4;
        }
        
        if (curr_state != prev_state) {
            if ((curr_state & (1 << 0)) && !(prev_state & (1 << 0))) {
                ESP_LOGI(TAG, "Input: UP");
                kraken_event_post(KRAKEN_EVENT_INPUT_UP, NULL, 0);
            }
            if ((curr_state & (1 << 1)) && !(prev_state & (1 << 1))) {
                ESP_LOGI(TAG, "Input: DOWN");
                kraken_event_post(KRAKEN_EVENT_INPUT_DOWN, NULL, 0);
            }
            if ((curr_state & (1 << 2)) && !(prev_state & (1 << 2))) {
                ESP_LOGI(TAG, "Input: LEFT");
                kraken_event_post(KRAKEN_EVENT_INPUT_LEFT, NULL, 0);
            }
            if ((curr_state & (1 << 3)) && !(prev_state & (1 << 3))) {
                ESP_LOGI(TAG, "Input: RIGHT");
                kraken_event_post(KRAKEN_EVENT_INPUT_RIGHT, NULL, 0);
            }
            if ((curr_state & (1 << 4)) && !(prev_state & (1 << 4))) {
                ESP_LOGI(TAG, "Input: CENTER");
                kraken_event_post(KRAKEN_EVENT_INPUT_CENTER, NULL, 0);
            }
            
            prev_state = curr_state;
        }
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    vTaskDelete(NULL);
}

esp_err_t system_service_init(void)
{
    if (g_system.initialized) {
        return ESP_OK;
    }

    g_system.input_cfg = board_support_get_input_config();

    kraken_event_subscribe(KRAKEN_EVENT_WIFI_GOT_IP, wifi_event_handler, NULL);

    g_system.initialized = true;
    ESP_LOGI(TAG, "System service initialized");
    return ESP_OK;
}

esp_err_t system_service_deinit(void)
{
    if (!g_system.initialized) {
        return ESP_OK;
    }

    if (g_system.input_monitor_running) {
        system_service_stop_input_monitor();
    }

    kraken_event_unsubscribe(KRAKEN_EVENT_WIFI_GOT_IP, wifi_event_handler);

    g_system.initialized = false;
    ESP_LOGI(TAG, "System service deinitialized");
    return ESP_OK;
}

esp_err_t system_service_sync_time(const char *ntp_server, const char *timezone)
{
    if (!g_system.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (timezone) {
        setenv("TZ", timezone, 1);
        tzset();
    }

    esp_sntp_setoperatingmode(SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, ntp_server ? ntp_server : "pool.ntp.org");
    esp_sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    esp_sntp_init();

    ESP_LOGI(TAG, "SNTP initialized for time sync");
    return ESP_OK;
}

esp_err_t system_service_get_time(struct tm *timeinfo)
{
    if (!g_system.initialized || !timeinfo) {
        return ESP_ERR_INVALID_ARG;
    }

    time_t now;
    time(&now);
    localtime_r(&now, timeinfo);

    if (timeinfo->tm_year < (2020 - 1900)) {
        return ESP_ERR_INVALID_STATE;
    }

    return ESP_OK;
}

esp_err_t system_service_start_input_monitor(void)
{
    if (!g_system.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_system.input_monitor_running) {
        return ESP_OK;
    }

    g_system.input_monitor_running = true;

    BaseType_t ret = xTaskCreate(input_monitor_task, "input_mon", 2048, NULL,
                                   5, &g_system.input_task_handle);
    if (ret != pdPASS) {
        g_system.input_monitor_running = false;
        ESP_LOGE(TAG, "Failed to create input monitor task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Input monitor started");
    return ESP_OK;
}

esp_err_t system_service_stop_input_monitor(void)
{
    if (!g_system.initialized || !g_system.input_monitor_running) {
        return ESP_OK;
    }

    g_system.input_monitor_running = false;

    if (g_system.input_task_handle) {
        vTaskDelay(pdMS_TO_TICKS(100));
        g_system.input_task_handle = NULL;
    }

    ESP_LOGI(TAG, "Input monitor stopped");
    return ESP_OK;
}
