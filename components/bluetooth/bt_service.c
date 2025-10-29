#include "kraken/bt_service.h"
#include "kraken/kernel.h"
#include "esp_log.h"
#include <string.h>

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "nvs_flash.h"
#endif

static const char *TAG = "bt_service";

#if CONFIG_BT_ENABLED
static struct {
    bool initialized;
    bool enabled;
    bool connected;
    bool scanning;
    bt_scan_result_t scan_results;
} g_bt = {0};

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            if (g_bt.scan_results.count < BT_MAX_SCAN_RESULTS) {
                bt_device_info_t *dev = &g_bt.scan_results.devices[g_bt.scan_results.count];
                memcpy(dev->mac, param->disc_res.bda, BT_MAC_ADDR_LEN);
                dev->rssi = -1;
                dev->name[0] = '\0';
                
                for (int i = 0; i < param->disc_res.num_prop; i++) {
                    esp_bt_gap_dev_prop_t *prop = &param->disc_res.prop[i];
                    switch (prop->type) {
                        case ESP_BT_GAP_DEV_PROP_BDNAME:
                            strncpy(dev->name, (char *)prop->val, BT_DEVICE_NAME_MAX_LEN - 1);
                            break;
                        case ESP_BT_GAP_DEV_PROP_RSSI:
                            dev->rssi = *(int8_t *)prop->val;
                            break;
                        default:
                            break;
                    }
                }
                
                g_bt.scan_results.count++;
                ESP_LOGI(TAG, "Found device: %s (RSSI: %d)", 
                         dev->name[0] ? dev->name : "Unknown", dev->rssi);
            }
            break;

        case ESP_BT_GAP_DISC_STATE_CHANGED_EVT:
            if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STOPPED) {
                g_bt.scanning = false;
                ESP_LOGI(TAG, "Discovery stopped");
                kraken_event_post(KRAKEN_EVENT_BT_SCAN_DONE, NULL, 0);
            } else if (param->disc_st_chg.state == ESP_BT_GAP_DISCOVERY_STARTED) {
                g_bt.scanning = true;
                ESP_LOGI(TAG, "Discovery started");
            }
            break;

        default:
            break;
    }
}

#endif // CONFIG_BT_ENABLED

esp_err_t bt_service_init(void)
{
#if CONFIG_BT_ENABLED
    if (g_bt.initialized) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(nvs_flash_init());

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));

    g_bt.initialized = true;
    ESP_LOGI(TAG, "BT service initialized");
    return ESP_OK;
#else
    ESP_LOGW(TAG, "BT service disabled (not supported on this platform)");
    return ESP_OK;
#endif
}

esp_err_t bt_service_deinit(void)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized) {
        return ESP_OK;
    }

    if (g_bt.enabled) {
        bt_service_disable();
    }

    esp_bt_controller_deinit();

    g_bt.initialized = false;
    ESP_LOGI(TAG, "BT service deinitialized");
    return ESP_OK;
#else
    return ESP_OK;
#endif
}

esp_err_t bt_service_enable(void)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    if (g_bt.enabled) {
        return ESP_OK;
    }

    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_bt_gap_register_callback(gap_callback));

    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    g_bt.enabled = true;
    ESP_LOGI(TAG, "BT enabled");
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t bt_service_disable(void)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized || !g_bt.enabled) {
        return ESP_OK;
    }

    g_bt.scanning = false;

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();

    g_bt.enabled = false;
    g_bt.connected = false;
    ESP_LOGI(TAG, "BT disabled");
    return ESP_OK;
#else
    return ESP_OK;
#endif
}

bool bt_service_is_enabled(void)
{
#if CONFIG_BT_ENABLED
    return g_bt.enabled;
#else
    return false;
#endif
}

esp_err_t bt_service_scan(uint32_t duration_sec)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized || !g_bt.enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    memset(&g_bt.scan_results, 0, sizeof(g_bt.scan_results));

    esp_bt_inq_mode_t mode = ESP_BT_INQ_MODE_GENERAL_INQUIRY;
    uint8_t inq_len = duration_sec < 1 ? 1 : (duration_sec > 48 ? 48 : duration_sec);
    uint8_t num_rsps = 0;

    esp_err_t ret = esp_bt_gap_start_discovery(mode, inq_len, num_rsps);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start discovery: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "BT scan started for %d seconds", inq_len);
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t bt_service_get_scan_results(bt_scan_result_t *results)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized || !results) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(results, &g_bt.scan_results, sizeof(bt_scan_result_t));
    ESP_LOGI(TAG, "Retrieved %d BT devices", results->count);
    return ESP_OK;
#else
    if (results) {
        memset(results, 0, sizeof(bt_scan_result_t));
    }
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t bt_service_connect(const uint8_t *mac)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized || !g_bt.enabled || !mac) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "BT connect requested (not fully implemented)");
    return ESP_OK;
#else
    return ESP_ERR_NOT_SUPPORTED;
#endif
}

esp_err_t bt_service_disconnect(void)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized || !g_bt.enabled) {
        return ESP_OK;
    }

    g_bt.connected = false;
    ESP_LOGI(TAG, "BT disconnected");
    return ESP_OK;
#else
    return ESP_OK;
#endif
}

bool bt_service_is_connected(void)
{
#if CONFIG_BT_ENABLED
    return g_bt.connected;
#else
    return false;
#endif
}
