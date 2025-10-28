#include <stdio.h>
#include "kraken/kernel.h"
#include "kraken/bsp.h"
#include "kraken/wifi_service.h"
#include "kraken/bt_service.h"
#include "kraken/display_service.h"
#include "kraken/system_service.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "kraken";

void app_main(void)
{
    ESP_LOGI(TAG, "Kraken OS starting...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(board_support_init());

    ESP_ERROR_CHECK(kraken_kernel_init());

    ESP_ERROR_CHECK(kraken_service_register("wifi", 
                                             KRAKEN_PERM_WIFI | KRAKEN_PERM_NETWORK,
                                             wifi_service_init,
                                             wifi_service_deinit));

    ESP_ERROR_CHECK(kraken_service_register("bluetooth",
                                             KRAKEN_PERM_BT,
                                             bt_service_init,
                                             bt_service_deinit));

    ESP_ERROR_CHECK(kraken_service_register("display",
                                             KRAKEN_PERM_DISPLAY,
                                             display_service_init,
                                             display_service_deinit));

    ESP_ERROR_CHECK(kraken_service_register("system",
                                             KRAKEN_PERM_SYSTEM | KRAKEN_PERM_ALL,
                                             system_service_init,
                                             system_service_deinit));

    ESP_ERROR_CHECK(kraken_service_start("system"));
    ESP_ERROR_CHECK(kraken_service_start("display"));
    ESP_ERROR_CHECK(kraken_service_start("wifi"));
    
    ESP_ERROR_CHECK(system_service_start_input_monitor());

    ESP_LOGI(TAG, "Kraken OS started successfully");
    ESP_LOGI(TAG, "Free heap: %d bytes", kraken_get_free_heap_size());

    display_lock();
    lv_obj_t *label = display_create_label(NULL, "Kraken OS\nReady!");
    lv_obj_align(label, LV_ALIGN_CENTER, 0, -40);
    
    lv_obj_t *heap_label = display_create_label(NULL, "");
    lv_obj_align(heap_label, LV_ALIGN_CENTER, 0, 0);
    char heap_str[64];
    snprintf(heap_str, sizeof(heap_str), "Free heap: %d KB", 
             kraken_get_free_heap_size() / 1024);
    lv_label_set_text(heap_label, heap_str);
    display_unlock();
}

