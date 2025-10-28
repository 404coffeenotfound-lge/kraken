#include "kraken/kernel.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

esp_err_t kraken_timer_create(const char *name, uint32_t period_ms,
                               bool auto_reload, void (*callback)(void*),
                               void *arg, void **handle)
{
    if (!name || !callback || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    TimerHandle_t timer = xTimerCreate(name, pdMS_TO_TICKS(period_ms),
                                        auto_reload ? pdTRUE : pdFALSE,
                                        arg, (TimerCallbackFunction_t)callback);
    if (!timer) {
        return ESP_ERR_NO_MEM;
    }

    *handle = timer;
    return ESP_OK;
}

esp_err_t kraken_timer_start(void *handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xTimerStart((TimerHandle_t)handle, pdMS_TO_TICKS(100)) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t kraken_timer_stop(void *handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xTimerStop((TimerHandle_t)handle, pdMS_TO_TICKS(100)) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

esp_err_t kraken_timer_delete(void *handle)
{
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xTimerDelete((TimerHandle_t)handle, pdMS_TO_TICKS(100)) != pdPASS) {
        return ESP_FAIL;
    }

    return ESP_OK;
}

uint32_t kraken_get_tick_count(void)
{
    return (uint32_t)(xTaskGetTickCount() * portTICK_PERIOD_MS);
}

void kraken_delay_ms(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}
