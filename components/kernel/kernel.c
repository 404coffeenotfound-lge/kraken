#include "kernel_internal.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "kraken_kernel";

kernel_state_t g_kernel = {0};

esp_err_t kraken_kernel_init(void)
{
    if (g_kernel.initialized) {
        return ESP_OK;
    }

    memset(&g_kernel, 0, sizeof(g_kernel));

    esp_err_t ret = kernel_service_init();
    if (ret != ESP_OK) {
        return ret;
    }

    ret = kernel_event_init();
    if (ret != ESP_OK) {
        kernel_service_cleanup();
        return ret;
    }

    g_kernel.initialized = true;
    ESP_LOGI(TAG, "Kernel initialized");
    return ESP_OK;
}

esp_err_t kraken_kernel_deinit(void)
{
    if (!g_kernel.initialized) {
        return ESP_OK;
    }

    kernel_event_cleanup();
    kernel_service_cleanup();

    memset(&g_kernel, 0, sizeof(g_kernel));
    ESP_LOGI(TAG, "Kernel deinitialized");
    return ESP_OK;
}
