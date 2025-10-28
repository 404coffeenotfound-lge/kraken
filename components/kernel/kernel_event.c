#include "kernel_internal.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "kernel_evt";

esp_err_t kernel_event_init(void)
{
    g_kernel.event_mutex = xSemaphoreCreateMutex();
    if (!g_kernel.event_mutex) {
        ESP_LOGE(TAG, "Failed to create event mutex");
        return ESP_ERR_NO_MEM;
    }

    g_kernel.event_queue = xQueueCreate(32, sizeof(kraken_event_t));
    if (!g_kernel.event_queue) {
        ESP_LOGE(TAG, "Failed to create event queue");
        vSemaphoreDelete(g_kernel.event_mutex);
        return ESP_ERR_NO_MEM;
    }

    BaseType_t ret = xTaskCreate(kernel_event_task, "kraken_evt", 3072, NULL, 5, &g_kernel.event_task);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create event task");
        vQueueDelete(g_kernel.event_queue);
        vSemaphoreDelete(g_kernel.event_mutex);
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}

void kernel_event_cleanup(void)
{
    if (g_kernel.event_task) {
        vTaskDelete(g_kernel.event_task);
        g_kernel.event_task = NULL;
    }
    if (g_kernel.event_queue) {
        vQueueDelete(g_kernel.event_queue);
        g_kernel.event_queue = NULL;
    }
    if (g_kernel.event_mutex) {
        vSemaphoreDelete(g_kernel.event_mutex);
        g_kernel.event_mutex = NULL;
    }
}

esp_err_t kraken_event_subscribe(kraken_event_type_t event_type,
                                  kraken_event_handler_t handler,
                                  void *user_data)
{
    if (!g_kernel.initialized || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_kernel.event_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (g_kernel.listener_count >= KRAKEN_MAX_EVENT_LISTENERS) {
        xSemaphoreGive(g_kernel.event_mutex);
        ESP_LOGE(TAG, "Max event listeners reached");
        return ESP_ERR_NO_MEM;
    }

    event_listener_t *listener = &g_kernel.listeners[g_kernel.listener_count];
    listener->event_type = event_type;
    listener->handler = handler;
    listener->user_data = user_data;

    g_kernel.listener_count++;
    xSemaphoreGive(g_kernel.event_mutex);

    ESP_LOGD(TAG, "Event %d subscribed", event_type);
    return ESP_OK;
}

esp_err_t kraken_event_unsubscribe(kraken_event_type_t event_type,
                                    kraken_event_handler_t handler)
{
    if (!g_kernel.initialized || !handler) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_kernel.event_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (uint8_t i = 0; i < g_kernel.listener_count; i++) {
        if (g_kernel.listeners[i].event_type == event_type &&
            g_kernel.listeners[i].handler == handler) {
            for (uint8_t j = i; j < g_kernel.listener_count - 1; j++) {
                g_kernel.listeners[j] = g_kernel.listeners[j + 1];
            }
            g_kernel.listener_count--;
            xSemaphoreGive(g_kernel.event_mutex);
            ESP_LOGD(TAG, "Event %d unsubscribed", event_type);
            return ESP_OK;
        }
    }

    xSemaphoreGive(g_kernel.event_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t kraken_event_post(kraken_event_type_t event_type, void *data, uint32_t data_len)
{
    if (!g_kernel.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    kraken_event_t evt = {
        .type = event_type,
        .data = data,
        .data_len = data_len,
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
    };

    if (xQueueSend(g_kernel.event_queue, &evt, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGW(TAG, "Event queue full, event %d dropped", event_type);
        return ESP_ERR_TIMEOUT;
    }

    return ESP_OK;
}

esp_err_t kraken_event_post_from_isr(kraken_event_type_t event_type, void *data, uint32_t data_len)
{
    if (!g_kernel.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    kraken_event_t evt = {
        .type = event_type,
        .data = data,
        .data_len = data_len,
        .timestamp = (uint32_t)(esp_timer_get_time() / 1000),
    };

    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (xQueueSendFromISR(g_kernel.event_queue, &evt, &xHigherPriorityTaskWoken) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }

    return ESP_OK;
}

void kernel_event_task(void *arg)
{
    kraken_event_t evt;

    while (1) {
        if (xQueueReceive(g_kernel.event_queue, &evt, portMAX_DELAY) == pdTRUE) {
            if (xSemaphoreTake(g_kernel.event_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                for (uint8_t i = 0; i < g_kernel.listener_count; i++) {
                    if (g_kernel.listeners[i].event_type == evt.type ||
                        g_kernel.listeners[i].event_type == KRAKEN_EVENT_NONE) {
                        g_kernel.listeners[i].handler(&evt, g_kernel.listeners[i].user_data);
                    }
                }
                xSemaphoreGive(g_kernel.event_mutex);
            }
        }
    }
}
