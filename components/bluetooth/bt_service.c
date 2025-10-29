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
    bt_device_type_t connected_device_type;  // Track connected device type
} g_bt = {0};

// Helper: Convert device type to string
static const char* bt_device_type_to_string(bt_device_type_t type)
{
    switch (type) {
        case BT_DEVICE_TYPE_PHONE: return "Phone";
        case BT_DEVICE_TYPE_COMPUTER: return "Computer";
        case BT_DEVICE_TYPE_HEADSET: return "Headset";
        case BT_DEVICE_TYPE_SPEAKER: return "Speaker";
        case BT_DEVICE_TYPE_KEYBOARD: return "Keyboard";
        case BT_DEVICE_TYPE_MOUSE: return "Mouse";
        case BT_DEVICE_TYPE_GAMEPAD: return "Gamepad";
        case BT_DEVICE_TYPE_GPS: return "GPS";
        case BT_DEVICE_TYPE_SERIAL: return "Serial";
        default: return "Unknown";
    }
}

// Helper: Detect device type from Class of Device (CoD) and name
static bt_device_type_t bt_detect_device_type(uint32_t cod, const char *name)
{
    // Bluetooth Class of Device format: 0x00MMMMSS
    // Major Device Class (bits 8-12)
    uint8_t major_class = (cod >> 8) & 0x1F;
    // Minor Device Class (bits 2-7)
    uint8_t minor_class = (cod >> 2) & 0x3F;
    
    // Check name patterns first (more reliable)
    if (name && name[0]) {
        const char *name_lower = name;
        
        // Phone patterns
        if (strstr(name_lower, "Phone") || strstr(name_lower, "iPhone") || 
            strstr(name_lower, "Android") || strstr(name_lower, "Galaxy") ||
            strstr(name_lower, "Pixel")) {
            return BT_DEVICE_TYPE_PHONE;
        }
        
        // Computer patterns
        if (strstr(name_lower, "MacBook") || strstr(name_lower, "Laptop") ||
            strstr(name_lower, "PC") || strstr(name_lower, "Desktop") ||
            strstr(name_lower, "iMac")) {
            return BT_DEVICE_TYPE_COMPUTER;
        }
        
        // Keyboard patterns
        if (strstr(name_lower, "Keyboard") || strstr(name_lower, "keyboard")) {
            return BT_DEVICE_TYPE_KEYBOARD;
        }
        
        // Mouse patterns
        if (strstr(name_lower, "Mouse") || strstr(name_lower, "mouse") ||
            strstr(name_lower, "Trackpad")) {
            return BT_DEVICE_TYPE_MOUSE;
        }
        
        // Gamepad/Controller patterns
        if (strstr(name_lower, "Controller") || strstr(name_lower, "Gamepad") ||
            strstr(name_lower, "Joy") || strstr(name_lower, "Xbox") ||
            strstr(name_lower, "PlayStation") || strstr(name_lower, "Switch")) {
            return BT_DEVICE_TYPE_GAMEPAD;
        }
        
        // Headset patterns
        if (strstr(name_lower, "Headset") || strstr(name_lower, "Earbuds") ||
            strstr(name_lower, "AirPods") || strstr(name_lower, "Buds")) {
            return BT_DEVICE_TYPE_HEADSET;
        }
        
        // Speaker patterns
        if (strstr(name_lower, "Speaker") || strstr(name_lower, "Sound")) {
            return BT_DEVICE_TYPE_SPEAKER;
        }
        
        // GPS patterns
        if (strstr(name_lower, "GPS") || strstr(name_lower, "Navigator")) {
            return BT_DEVICE_TYPE_GPS;
        }
        
        // Serial/Arduino patterns
        if (strstr(name_lower, "Arduino") || strstr(name_lower, "ESP") ||
            strstr(name_lower, "Serial") || strstr(name_lower, "HC-")) {
            return BT_DEVICE_TYPE_SERIAL;
        }
    }
    
    // Fall back to Class of Device
    switch (major_class) {
        case 0x01:  // Computer
            return BT_DEVICE_TYPE_COMPUTER;
            
        case 0x02:  // Phone
            return BT_DEVICE_TYPE_PHONE;
            
        case 0x04:  // Audio/Video
            switch (minor_class) {
                case 0x01:  // Wearable Headset
                case 0x02:  // Hands-free
                    return BT_DEVICE_TYPE_HEADSET;
                case 0x05:  // Loudspeaker
                case 0x06:  // Portable Audio
                    return BT_DEVICE_TYPE_SPEAKER;
                default:
                    return BT_DEVICE_TYPE_HEADSET;
            }
            
        case 0x05:  // Peripheral (HID)
            switch (minor_class) {
                case 0x10:  // Keyboard
                case 0x11:  // Keyboard
                    return BT_DEVICE_TYPE_KEYBOARD;
                case 0x20:  // Pointing device (mouse)
                case 0x21:  // Pointing device
                    return BT_DEVICE_TYPE_MOUSE;
                case 0x08:  // Gamepad/Joystick
                case 0x09:  // Joystick
                    return BT_DEVICE_TYPE_GAMEPAD;
                default:
                    return BT_DEVICE_TYPE_KEYBOARD;
            }
            
        default:
            return BT_DEVICE_TYPE_UNKNOWN;
    }
}

// Helper: Auto-select and enable appropriate profiles based on device type
static void bt_auto_select_profiles(bt_device_type_t device_type)
{
    ESP_LOGI(TAG, "Auto-selecting profiles for device type: %s", bt_device_type_to_string(device_type));
    
    // Disable all profiles first (optional - or keep default ones)
    // For now, we'll keep SPP enabled for all devices as fallback
    
    switch (device_type) {
        case BT_DEVICE_TYPE_PHONE:
        case BT_DEVICE_TYPE_COMPUTER:
            // Enable audio and media control
            bt_profile_enable(BT_PROFILE_A2DP_SINK);
            bt_profile_enable(BT_PROFILE_AVRCP);
            bt_profile_enable(BT_PROFILE_SPP);  // For data transfer
            ESP_LOGI(TAG, "Enabled: A2DP + AVRCP + SPP (Audio device)");
            break;
            
        case BT_DEVICE_TYPE_HEADSET:
        case BT_DEVICE_TYPE_SPEAKER:
            // Enable audio profiles
            bt_profile_enable(BT_PROFILE_A2DP_SINK);
            bt_profile_enable(BT_PROFILE_AVRCP);
            bt_profile_enable(BT_PROFILE_HFP_CLIENT);
            ESP_LOGI(TAG, "Enabled: A2DP + AVRCP + HFP (Headset/Speaker)");
            break;
            
        case BT_DEVICE_TYPE_KEYBOARD:
        case BT_DEVICE_TYPE_MOUSE:
        case BT_DEVICE_TYPE_GAMEPAD:
            // Enable HID Host to receive input
            bt_profile_enable(BT_PROFILE_HID_HOST);
            ESP_LOGI(TAG, "Enabled: HID Host (Input device)");
            break;
            
        case BT_DEVICE_TYPE_GPS:
        case BT_DEVICE_TYPE_SERIAL:
            // Enable serial communication only
            bt_profile_enable(BT_PROFILE_SPP);
            ESP_LOGI(TAG, "Enabled: SPP (Serial device)");
            break;
            
        default:
            // Unknown device - enable all profiles
            bt_profile_enable(BT_PROFILE_SPP);
            bt_profile_enable(BT_PROFILE_A2DP_SINK);
            bt_profile_enable(BT_PROFILE_AVRCP);
            bt_profile_enable(BT_PROFILE_HID_HOST);
            ESP_LOGI(TAG, "Enabled: All profiles (Unknown device)");
            break;
    }
}

static void gap_callback(esp_bt_gap_cb_event_t event, esp_bt_gap_cb_param_t *param)
{
    switch (event) {
        case ESP_BT_GAP_DISC_RES_EVT:
            if (g_bt.scan_results.count < BT_MAX_SCAN_RESULTS) {
                bt_device_info_t *dev = &g_bt.scan_results.devices[g_bt.scan_results.count];
                memcpy(dev->mac, param->disc_res.bda, BT_MAC_ADDR_LEN);
                dev->rssi = -1;
                dev->name[0] = '\0';
                dev->device_type = BT_DEVICE_TYPE_UNKNOWN;
                dev->class_of_device = 0;
                
                for (int i = 0; i < param->disc_res.num_prop; i++) {
                    esp_bt_gap_dev_prop_t *prop = &param->disc_res.prop[i];
                    switch (prop->type) {
                        case ESP_BT_GAP_DEV_PROP_BDNAME:
                            strncpy(dev->name, (char *)prop->val, BT_DEVICE_NAME_MAX_LEN - 1);
                            break;
                        case ESP_BT_GAP_DEV_PROP_RSSI:
                            dev->rssi = *(int8_t *)prop->val;
                            break;
                        case ESP_BT_GAP_DEV_PROP_COD:
                            dev->class_of_device = *(uint32_t *)prop->val;
                            // Detect device type from Class of Device
                            dev->device_type = bt_detect_device_type(dev->class_of_device, dev->name);
                            break;
                        default:
                            break;
                    }
                }
                
                g_bt.scan_results.count++;
                const char *type_str = bt_device_type_to_string(dev->device_type);
                ESP_LOGI(TAG, "Found device: %s (RSSI: %d, Type: %s, CoD: 0x%06x)", 
                         dev->name[0] ? dev->name : "Unknown", dev->rssi, type_str, dev->class_of_device);
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
    bt_profile_enable(BT_PROFILE_SPP);  // Serial Port Profile
    bt_profile_enable(BT_PROFILE_A2DP_SINK);  // Audio Sink
    bt_profile_enable(BT_PROFILE_AVRCP);  // Media Control
    bt_profile_enable(BT_PROFILE_HID_HOST);  // HID input devices

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
    
    // Log device type if available (for debugging/info only)
    bt_device_type_t device_type = BT_DEVICE_TYPE_UNKNOWN;
    for (int i = 0; i < g_bt.scan_results.count; i++) {
        if (memcmp(g_bt.scan_results.devices[i].mac, mac, BT_MAC_ADDR_LEN) == 0) {
            device_type = g_bt.scan_results.devices[i].device_type;
            g_bt.connected_device_type = device_type;
            ESP_LOGI(TAG, "Connecting to %s (Detected type: %s)", mac_str, bt_device_type_to_string(device_type));
            break;
        }
    }
    
    if (device_type == BT_DEVICE_TYPE_UNKNOWN) {
        ESP_LOGI(TAG, "Connecting to %s (Type: Unknown)", mac_str);
    }

    // NOTE: We do NOT need to select profiles here!
    // All profile servers are already enabled (registered in SDP)
    // The remote device will query our SDP and connect to the profiles it needs
    // This is the standard Bluetooth way - the CLIENT chooses, not the SERVER
    
    ESP_LOGI(TAG, "All profiles available - remote device will select via SDP");

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
