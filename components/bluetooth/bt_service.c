#include "kraken/bt_service.h"
#include "kraken/bt_profiles.h"
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
    bool connecting;
    uint8_t remote_bda[BT_MAC_ADDR_LEN];
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

        case ESP_BT_GAP_AUTH_CMPL_EVT:
            if (param->auth_cmpl.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "Authentication success: %s", param->auth_cmpl.device_name);
                g_bt.connected = true;
                g_bt.connecting = false;
                memcpy(g_bt.remote_bda, param->auth_cmpl.bda, BT_MAC_ADDR_LEN);
                kraken_event_post(KRAKEN_EVENT_BT_CONNECTED, NULL, 0);
            } else {
                ESP_LOGE(TAG, "Authentication failed, status: %d", param->auth_cmpl.stat);
                g_bt.connected = false;
                g_bt.connecting = false;
                kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            }
            break;

        case ESP_BT_GAP_ACL_CONN_CMPL_STAT_EVT:
            if (param->acl_conn_cmpl_stat.stat == ESP_BT_STATUS_SUCCESS) {
                ESP_LOGI(TAG, "ACL connection established");
            } else {
                ESP_LOGE(TAG, "ACL connection failed");
                g_bt.connected = false;
                g_bt.connecting = false;
                kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            }
            break;

        case ESP_BT_GAP_ACL_DISCONN_CMPL_STAT_EVT:
            ESP_LOGI(TAG, "ACL disconnected");
            g_bt.connected = false;
            g_bt.connecting = false;
            memset(g_bt.remote_bda, 0, BT_MAC_ADDR_LEN);
            kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            break;

        case ESP_BT_GAP_PIN_REQ_EVT:
            ESP_LOGI(TAG, "PIN code request");
            // Use default PIN "0000" for simplicity
            esp_bt_pin_code_t pin_code = {'0', '0', '0', '0'};
            esp_bt_gap_pin_reply(param->pin_req.bda, true, 4, pin_code);
            break;

        case ESP_BT_GAP_CFM_REQ_EVT:
            ESP_LOGI(TAG, "Confirm request: %d", param->cfm_req.num_val);
            // Auto-confirm for simplicity
            esp_bt_gap_ssp_confirm_reply(param->cfm_req.bda, true);
            break;

        case ESP_BT_GAP_KEY_NOTIF_EVT:
            ESP_LOGI(TAG, "Passkey notification: %d", param->key_notif.passkey);
            break;

        case ESP_BT_GAP_KEY_REQ_EVT:
            ESP_LOGI(TAG, "Passkey request");
            break;

        default:
            ESP_LOGD(TAG, "Unhandled GAP event: %d", event);
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

    // Set device name
    esp_bt_dev_set_device_name("Kraken Device");

    // Set scan mode (connectable and discoverable)
    esp_bt_gap_set_scan_mode(ESP_BT_CONNECTABLE, ESP_BT_GENERAL_DISCOVERABLE);

    // Set default PIN code
    esp_bt_pin_type_t pin_type = ESP_BT_PIN_TYPE_FIXED;
    esp_bt_pin_code_t pin_code = {'0', '0', '0', '0'};
    esp_bt_gap_set_pin(pin_type, 4, pin_code);

    // Set SSP parameters (Simple Secure Pairing)
    esp_bt_sp_param_t param_type = ESP_BT_SP_IOCAP_MODE;
    esp_bt_io_cap_t iocap = ESP_BT_IO_CAP_NONE;  // No input/output capability
    esp_bt_gap_set_security_param(param_type, &iocap, sizeof(uint8_t));

    // Initialize profile manager
    bt_profiles_init();
    
    // Enable commonly used profiles by default
    bt_profile_enable(BT_PROFILE_SPP);         // Serial Port Profile
    bt_profile_enable(BT_PROFILE_A2DP_SINK);   // Audio Sink (receive audio)
    bt_profile_enable(BT_PROFILE_A2DP_SOURCE); // Audio Source (send to headphones)
    bt_profile_enable(BT_PROFILE_AVRCP);       // Media Control
    bt_profile_enable(BT_PROFILE_HID_HOST);    // HID input devices

    g_bt.enabled = true;
    ESP_LOGI(TAG, "BT enabled with profiles");
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
        esp_bt_gap_cancel_discovery();
        g_bt.scanning = false;
    }

    // Deinitialize profiles
    bt_profiles_deinit();

    esp_bluedroid_disable();
    esp_bluedroid_deinit();
    esp_bt_controller_disable();

    g_bt.enabled = false;
    g_bt.connected = false;
    g_bt.connecting = false;
    memset(g_bt.remote_bda, 0, BT_MAC_ADDR_LEN);
    
    ESP_LOGI(TAG, "BT disabled with profiles");
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

    if (g_bt.connecting || g_bt.connected) {
        ESP_LOGW(TAG, "Already connecting or connected");
        return ESP_ERR_INVALID_STATE;
    }

    // Stop discovery if running
    if (g_bt.scanning) {
        esp_bt_gap_cancel_discovery();
        g_bt.scanning = false;
    }

    // Store remote device address
    memcpy(g_bt.remote_bda, mac, BT_MAC_ADDR_LEN);
    g_bt.connecting = true;

    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
    ESP_LOGI(TAG, "Connecting to %s", mac_str);

    // NOTE: All profile servers are already enabled (registered in SDP)
    // The remote device will query our SDP and connect to the profiles it needs
    // This is the standard Bluetooth way - the CLIENT chooses, not the SERVER

    // Initiate authentication which will trigger connection
    esp_err_t ret = esp_bt_gap_set_pin(ESP_BT_PIN_TYPE_FIXED, 4, (esp_bt_pin_code_t){'0', '0', '0', '0'});
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set PIN: %d", ret);
        g_bt.connecting = false;
        return ret;
    }

    ESP_LOGI(TAG, "Connection initiated - waiting for remote device to select profiles");
    
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

    if (g_bt.connected) {
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                g_bt.remote_bda[0], g_bt.remote_bda[1], g_bt.remote_bda[2],
                g_bt.remote_bda[3], g_bt.remote_bda[4], g_bt.remote_bda[5]);
        ESP_LOGI(TAG, "Disconnecting from %s", mac_str);
        
        // Remove bonded device
        esp_bt_gap_remove_bond_device(g_bt.remote_bda);
    }

    g_bt.connected = false;
    g_bt.connecting = false;
    memset(g_bt.remote_bda, 0, BT_MAC_ADDR_LEN);
    
    ESP_LOGI(TAG, "BT disconnected");
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
