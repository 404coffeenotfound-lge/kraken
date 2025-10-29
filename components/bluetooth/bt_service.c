#include "kraken/bt_service.h"
#include "kraken/bt_profiles.h"
#include "kraken/kernel.h"
#include "esp_log.h"
#include <string.h>

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "nvs_flash.h"
#endif

static const char *TAG = "bt_service";

#if CONFIG_BT_ENABLED
static struct {
    bool initialized;
    bool enabled;
    bool connected;
    bool scanning;
    bool connecting;
    uint8_t remote_bda[BT_MAC_ADDR_LEN];
    esp_ble_addr_type_t remote_addr_type;
    bt_scan_result_t scan_results;
    uint32_t scan_duration;
    esp_gatt_if_t gattc_if;
    uint16_t conn_id;
    uint16_t mtu;
} g_bt = {0};

static void gattc_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    switch (event) {
        case ESP_GATTC_REG_EVT:
            ESP_LOGI(TAG, "GATT client registered, app_id=%d, status=%d", param->reg.app_id, param->reg.status);
            if (param->reg.status == ESP_GATT_OK) {
                g_bt.gattc_if = gattc_if;
            }
            break;
            
        case ESP_GATTC_OPEN_EVT:
            if (param->open.status == ESP_GATT_OK) {
                g_bt.conn_id = param->open.conn_id;
                g_bt.connected = true;
                g_bt.connecting = false;
                memcpy(g_bt.remote_bda, param->open.remote_bda, BT_MAC_ADDR_LEN);
                ESP_LOGI(TAG, "BLE GATT connected, conn_id=%d, MTU=%d", g_bt.conn_id, param->open.mtu);
                
                // Request MTU update to max
                esp_ble_gattc_send_mtu_req(gattc_if, param->open.conn_id);
                
                kraken_event_post(KRAKEN_EVENT_BT_CONNECTED, NULL, 0);
            } else {
                ESP_LOGE(TAG, "BLE connection failed, status=%d", param->open.status);
                g_bt.connected = false;
                g_bt.connecting = false;
                kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            }
            break;
            
        case ESP_GATTC_CLOSE_EVT:
            ESP_LOGI(TAG, "BLE GATT disconnected, reason=%d", param->close.reason);
            g_bt.connected = false;
            g_bt.connecting = false;
            g_bt.conn_id = 0;
            memset(g_bt.remote_bda, 0, BT_MAC_ADDR_LEN);
            kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            break;
            
        case ESP_GATTC_CFG_MTU_EVT:
            if (param->cfg_mtu.status == ESP_GATT_OK) {
                g_bt.mtu = param->cfg_mtu.mtu;
                ESP_LOGI(TAG, "MTU configured: %d", g_bt.mtu);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "GATT client event: %d", event);
            break;
    }
}

static void ble_gap_callback(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
            if (param->scan_param_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "BLE scan parameters set, starting scan...");
                esp_ble_gap_start_scanning(g_bt.scan_duration);
            } else {
                ESP_LOGE(TAG, "Failed to set BLE scan parameters: %d", param->scan_param_cmpl.status);
            }
            break;
            
        case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
            if (param->scan_start_cmpl.status == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "BLE scan started successfully");
                g_bt.scanning = true;
            } else {
                ESP_LOGE(TAG, "BLE scan start failed: %d", param->scan_start_cmpl.status);
            }
            break;
            
        case ESP_GAP_BLE_SCAN_RESULT_EVT:
            if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
                // Check if device already exists (deduplicate by MAC address)
                bool already_exists = false;
                int existing_index = -1;
                for (int i = 0; i < g_bt.scan_results.count; i++) {
                    if (memcmp(g_bt.scan_results.devices[i].mac, param->scan_rst.bda, BT_MAC_ADDR_LEN) == 0) {
                        already_exists = true;
                        existing_index = i;
                        break;
                    }
                }
                
                // If device exists, only update if we get a name or better RSSI
                if (already_exists) {
                    bt_device_info_t *dev = &g_bt.scan_results.devices[existing_index];
                    
                    // Update RSSI if better
                    if (param->scan_rst.rssi > dev->rssi) {
                        dev->rssi = param->scan_rst.rssi;
                    }
                    
                    // Try to get name if we don't have one yet (or if current name is MAC-based)
                    bool is_mac_name = (strncmp(dev->name, "BLE_", 4) == 0);
                    if (dev->name[0] == '\0' || is_mac_name) {
                        uint8_t *adv_name = NULL;
                        uint8_t adv_name_len = 0;
                        uint16_t total_len = param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len;
                        
                        // Try complete name first
                        adv_name = esp_ble_resolve_adv_data_by_type(param->scan_rst.ble_adv,
                                                                    total_len,
                                                                    ESP_BLE_AD_TYPE_NAME_CMPL,
                                                                    &adv_name_len);
                        // Try short name if complete name not found
                        if (!adv_name) {
                            adv_name = esp_ble_resolve_adv_data_by_type(param->scan_rst.ble_adv,
                                                                        total_len,
                                                                        ESP_BLE_AD_TYPE_NAME_SHORT,
                                                                        &adv_name_len);
                        }
                        
                        if (adv_name && adv_name_len > 0) {
                            size_t copy_len = adv_name_len < (BT_DEVICE_NAME_MAX_LEN - 1) ? 
                                             adv_name_len : (BT_DEVICE_NAME_MAX_LEN - 1);
                            char old_name[BT_DEVICE_NAME_MAX_LEN];
                            strncpy(old_name, dev->name, sizeof(old_name));
                            memcpy(dev->name, adv_name, copy_len);
                            dev->name[copy_len] = '\0';
                            ESP_LOGI(TAG, "Updated device name: %s -> %s (RSSI: %d)", old_name, dev->name, dev->rssi);
                            kraken_event_post(KRAKEN_EVENT_BT_SCAN_DONE, NULL, 0);
                        }
                    }
                } else if (g_bt.scan_results.count < BT_MAX_SCAN_RESULTS) {
                    // New device - add it
                    bt_device_info_t *dev = &g_bt.scan_results.devices[g_bt.scan_results.count];
                    memcpy(dev->mac, param->scan_rst.bda, BT_MAC_ADDR_LEN);
                    dev->rssi = param->scan_rst.rssi;
                    dev->addr_type = param->scan_rst.ble_addr_type;  // Store address type
                    dev->name[0] = '\0';
                    
                    // Debug: Log raw advertisement data
                    char mac_str[18];
                    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                            param->scan_rst.bda[0], param->scan_rst.bda[1], param->scan_rst.bda[2],
                            param->scan_rst.bda[3], param->scan_rst.bda[4], param->scan_rst.bda[5]);
                    
                    ESP_LOGI(TAG, "Device %s: adv_len=%d, scan_rsp_len=%d, total=%d", 
                             mac_str, param->scan_rst.adv_data_len, param->scan_rst.scan_rsp_len,
                             param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len);
                    
                    // Dump advertisement data if present
                    if (param->scan_rst.adv_data_len > 0) {
                        ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->scan_rst.ble_adv, param->scan_rst.adv_data_len, ESP_LOG_INFO);
                    }
                    
                    // Dump scan response data if present
                    if (param->scan_rst.scan_rsp_len > 0) {
                        ESP_LOGI(TAG, "Scan response data:");
                        ESP_LOG_BUFFER_HEX_LEVEL(TAG, param->scan_rst.ble_adv + param->scan_rst.adv_data_len, 
                                                param->scan_rst.scan_rsp_len, ESP_LOG_INFO);
                    }
                    
                    // Try to get device name from combined advertisement + scan response data
                    // Using the total length allows the function to search through both ADV and SCAN_RSP
                    uint8_t *adv_name = NULL;
                    uint8_t adv_name_len = 0;
                    uint16_t total_len = param->scan_rst.adv_data_len + param->scan_rst.scan_rsp_len;
                    
                    // Try complete name first
                    adv_name = esp_ble_resolve_adv_data_by_type(param->scan_rst.ble_adv,
                                                                total_len,
                                                                ESP_BLE_AD_TYPE_NAME_CMPL,
                                                                &adv_name_len);
                    // Try short name if complete name not found
                    if (!adv_name) {
                        adv_name = esp_ble_resolve_adv_data_by_type(param->scan_rst.ble_adv,
                                                                    total_len,
                                                                    ESP_BLE_AD_TYPE_NAME_SHORT,
                                                                    &adv_name_len);
                    }
                    
                    if (adv_name && adv_name_len > 0) {
                        size_t copy_len = adv_name_len < (BT_DEVICE_NAME_MAX_LEN - 1) ? 
                                         adv_name_len : (BT_DEVICE_NAME_MAX_LEN - 1);
                        memcpy(dev->name, adv_name, copy_len);
                        dev->name[copy_len] = '\0';
                        ESP_LOGI(TAG, "✓ Found BLE device #%d: '%s' (RSSI: %d)", 
                                 g_bt.scan_results.count + 1, dev->name, dev->rssi);
                    } else {
                        // If no name in advertisement or scan response, create identifier from MAC address
                        snprintf(dev->name, BT_DEVICE_NAME_MAX_LEN, "BLE_%02X%02X%02X",
                                param->scan_rst.bda[3], param->scan_rst.bda[4], param->scan_rst.bda[5]);
                        ESP_LOGI(TAG, "✗ Found BLE device #%d: %s (RSSI: %d) [No name advertised]", 
                                 g_bt.scan_results.count + 1, dev->name, dev->rssi);
                    }
                    
                    g_bt.scan_results.count++;
                    
                    // Post event to update UI dynamically as devices are found
                    kraken_event_post(KRAKEN_EVENT_BT_SCAN_DONE, NULL, 0);
                }
            } else if (param->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_CMPL_EVT) {
                ESP_LOGI(TAG, "BLE scan complete, found %d unique devices", g_bt.scan_results.count);
                g_bt.scanning = false;
                kraken_event_post(KRAKEN_EVENT_BT_SCAN_DONE, NULL, 0);
            }
            break;
            
        case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
            ESP_LOGI(TAG, "BLE scan stopped");
            g_bt.scanning = false;
            kraken_event_post(KRAKEN_EVENT_BT_SCAN_DONE, NULL, 0);
            break;
            
        default:
            ESP_LOGD(TAG, "Unhandled BLE GAP event: %d", event);
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

    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    ESP_ERROR_CHECK(esp_ble_gap_register_callback(ble_gap_callback));
    
    // Register GATT client
    ESP_ERROR_CHECK(esp_ble_gattc_register_callback(gattc_event_handler));
    ESP_ERROR_CHECK(esp_ble_gattc_app_register(0));

    g_bt.enabled = true;
    ESP_LOGI(TAG, "BLE enabled with GATT client");
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

    // Disconnect if connected
    if (g_bt.connected || g_bt.connecting) {
        bt_service_disconnect();
    }

    // Stop scanning if active
    if (g_bt.scanning) {
        esp_ble_gap_stop_scanning();
        g_bt.scanning = false;
    }

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();

    g_bt.enabled = false;
    g_bt.connected = false;
    g_bt.connecting = false;
    memset(g_bt.remote_bda, 0, BT_MAC_ADDR_LEN);
    
    ESP_LOGI(TAG, "BLE disabled");
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
        ESP_LOGE(TAG, "BT not initialized or enabled");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "Clearing previous scan results (%d devices)", g_bt.scan_results.count);
    memset(&g_bt.scan_results, 0, sizeof(g_bt.scan_results));
    
    // Store scan duration for use in callback
    g_bt.scan_duration = duration_sec;

    // Set BLE scan parameters
    esp_ble_scan_params_t ble_scan_params = {
        .scan_type = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval = 0x50,
        .scan_window = 0x30,
        .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE
    };
    
    ESP_LOGI(TAG, "Setting BLE scan parameters...");
    esp_err_t ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set BLE scan params: 0x%x", ret);
        return ret;
    }
    
    // Scan will be started in ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT callback
    ESP_LOGI(TAG, "BLE scan param request sent, waiting for callback to start scan");
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

esp_err_t bt_service_connect(const uint8_t *mac, esp_ble_addr_type_t addr_type)
{
#if CONFIG_BT_ENABLED
    if (!g_bt.initialized || !g_bt.enabled || !mac) {
        return ESP_ERR_INVALID_ARG;
    }

    if (g_bt.connecting || g_bt.connected) {
        ESP_LOGW(TAG, "Already connecting or connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Stop scanning if active
    if (g_bt.scanning) {
        esp_ble_gap_stop_scanning();
        g_bt.scanning = false;
    }

    // Store remote device address and type
    memcpy(g_bt.remote_bda, mac, BT_MAC_ADDR_LEN);
    g_bt.remote_addr_type = addr_type;
    g_bt.connecting = true;

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    const char *addr_type_str = (addr_type == BLE_ADDR_TYPE_PUBLIC) ? "PUBLIC" : 
                                (addr_type == BLE_ADDR_TYPE_RANDOM) ? "RANDOM" : "UNKNOWN";
    ESP_LOGI(TAG, "Connecting to BLE device %s (addr_type: %s) via GATT", mac_str, addr_type_str);
    
    // Open BLE GATT connection with correct address type
    esp_err_t ret = esp_ble_gattc_open(g_bt.gattc_if, (uint8_t *)mac, addr_type, true);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open GATT connection: 0x%x", ret);
        g_bt.connecting = false;
        return ret;
    }
    
    ESP_LOGI(TAG, "GATT connection request sent");
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

    if (!g_bt.connected && !g_bt.connecting) {
        ESP_LOGW(TAG, "Not connected or connecting");
        return ESP_OK;
    }

    if (g_bt.connected && g_bt.conn_id != 0) {
        ESP_LOGI(TAG, "Closing BLE GATT connection");
        esp_ble_gattc_close(g_bt.gattc_if, g_bt.conn_id);
    }
    
    g_bt.connected = false;
    g_bt.connecting = false;
    g_bt.conn_id = 0;
    memset(g_bt.remote_bda, 0, BT_MAC_ADDR_LEN);
    
    ESP_LOGI(TAG, "BLE disconnected");
    kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
    
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

// Stub implementations for bt_profiles (ESP32-S3 BLE-only - no Classic BT profiles)
esp_err_t bt_profiles_init(void)
{
    ESP_LOGW(TAG, "Classic BT profiles not supported on ESP32-S3 (BLE-only)");
    return ESP_OK;
}

esp_err_t bt_profiles_deinit(void)
{
    return ESP_OK;
}

esp_err_t bt_profile_enable(bt_profile_t profile)
{
    ESP_LOGW(TAG, "Classic BT profile %d not supported on ESP32-S3", profile);
    return ESP_ERR_NOT_SUPPORTED;
}

bool bt_profile_is_enabled(bt_profile_t profile)
{
    (void)profile;
    return false;
}

esp_err_t bt_a2dp_source_connect(const uint8_t *remote_bda)
{
    (void)remote_bda;
    ESP_LOGW(TAG, "A2DP Source not supported on ESP32-S3 (BLE-only)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bt_a2dp_source_disconnect(void)
{
    ESP_LOGW(TAG, "A2DP Source not supported on ESP32-S3 (BLE-only)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bt_a2dp_source_start_stream(void)
{
    ESP_LOGW(TAG, "A2DP Source not supported on ESP32-S3 (BLE-only)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bt_a2dp_source_stop_stream(void)
{
    ESP_LOGW(TAG, "A2DP Source not supported on ESP32-S3 (BLE-only)");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t bt_a2dp_source_write_data(const uint8_t *data, uint32_t len)
{
    (void)data;
    (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}
