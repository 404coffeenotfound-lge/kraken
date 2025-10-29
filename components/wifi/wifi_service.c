#include "kraken/wifi_service.h"
#include "kraken/kernel.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "nvs_flash.h"
#include <string.h>

static const char *TAG = "wifi_service";

static struct {
    bool initialized;
    bool enabled;
    bool connected;
    esp_netif_t *netif;
    wifi_scan_result_t scan_results;
} g_wifi = {0};

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                                int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "WiFi started");
                break;
            case WIFI_EVENT_STA_STOP:
                ESP_LOGI(TAG, "WiFi stopped");
                g_wifi.connected = false;
                kraken_event_post(KRAKEN_EVENT_WIFI_DISCONNECTED, NULL, 0);
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "WiFi connected");
                break;
            case WIFI_EVENT_STA_DISCONNECTED:
                ESP_LOGI(TAG, "WiFi disconnected");
                g_wifi.connected = false;
                kraken_event_post(KRAKEN_EVENT_WIFI_DISCONNECTED, NULL, 0);
                break;
            case WIFI_EVENT_SCAN_DONE:
                ESP_LOGI(TAG, "WiFi scan done");
                kraken_event_post(KRAKEN_EVENT_WIFI_SCAN_DONE, NULL, 0);
                break;
        }
    } else if (event_base == IP_EVENT) {
        if (event_id == IP_EVENT_STA_GOT_IP) {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
            g_wifi.connected = true;
            kraken_event_post(KRAKEN_EVENT_WIFI_CONNECTED, NULL, 0);
            kraken_event_post(KRAKEN_EVENT_WIFI_GOT_IP, &event->ip_info, sizeof(event->ip_info));
        }
    }
}

esp_err_t wifi_service_init(void)
{
    if (g_wifi.initialized) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    g_wifi.netif = esp_netif_create_default_wifi_sta();
    if (!g_wifi.netif) {
        ESP_LOGE(TAG, "Failed to create netif");
        return ESP_FAIL;
    }

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, 
                                                 &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 &wifi_event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    g_wifi.initialized = true;
    ESP_LOGI(TAG, "WiFi service initialized");
    return ESP_OK;
}

esp_err_t wifi_service_deinit(void)
{
    if (!g_wifi.initialized) {
        return ESP_OK;
    }

    if (g_wifi.enabled) {
        wifi_service_disable();
    }

    esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler);
    esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler);

    esp_wifi_deinit();
    esp_netif_destroy(g_wifi.netif);

    g_wifi.initialized = false;
    ESP_LOGI(TAG, "WiFi service deinitialized");
    return ESP_OK;
}

esp_err_t wifi_service_enable(void)
{
    if (!g_wifi.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_wifi.enabled) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_start());
    g_wifi.enabled = true;
    ESP_LOGI(TAG, "WiFi enabled");
    return ESP_OK;
}

esp_err_t wifi_service_disable(void)
{
    if (!g_wifi.initialized || !g_wifi.enabled) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_stop());
    g_wifi.enabled = false;
    g_wifi.connected = false;
    ESP_LOGI(TAG, "WiFi disabled");
    return ESP_OK;
}

bool wifi_service_is_enabled(void)
{
    return g_wifi.enabled;
}

esp_err_t wifi_service_scan(void)
{
    // Note: No permission check - UI can initiate scan on behalf of user
    
    if (!g_wifi.initialized || !g_wifi.enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = false,
    };

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, false));
    ESP_LOGI(TAG, "WiFi scan started");
    return ESP_OK;
}

esp_err_t wifi_service_get_scan_results(wifi_scan_result_t *results)
{
    if (!g_wifi.initialized || !results) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t ap_count = WIFI_MAX_SCAN_RESULTS;
    wifi_ap_record_t ap_records[WIFI_MAX_SCAN_RESULTS];

    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&ap_count, ap_records));

    results->count = ap_count;
    for (uint16_t i = 0; i < ap_count; i++) {
        strncpy(results->aps[i].ssid, (char *)ap_records[i].ssid, WIFI_SSID_MAX_LEN - 1);
        results->aps[i].rssi = ap_records[i].rssi;
        results->aps[i].auth_mode = ap_records[i].authmode;
        results->aps[i].channel = ap_records[i].primary;
    }

    ESP_LOGI(TAG, "Found %d APs", ap_count);
    return ESP_OK;
}

esp_err_t wifi_service_connect(const char *ssid, const char *password)
{
    if (!g_wifi.initialized || !g_wifi.enabled || !ssid) {
        return ESP_ERR_INVALID_ARG;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    if (password) {
        strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);
    }

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_connect());

    ESP_LOGI(TAG, "Connecting to %s...", ssid);
    return ESP_OK;
}

esp_err_t wifi_service_disconnect(void)
{
    if (!g_wifi.initialized || !g_wifi.enabled) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_wifi_disconnect());
    g_wifi.connected = false;
    ESP_LOGI(TAG, "Disconnected");
    return ESP_OK;
}

bool wifi_service_is_connected(void)
{
    return g_wifi.connected;
}

esp_err_t wifi_service_get_ip(char *ip_str, size_t len)
{
    if (!g_wifi.initialized || !g_wifi.connected || !ip_str) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_netif_ip_info_t ip_info;
    ESP_ERROR_CHECK(esp_netif_get_ip_info(g_wifi.netif, &ip_info));

    snprintf(ip_str, len, IPSTR, IP2STR(&ip_info.ip));
    return ESP_OK;
}
