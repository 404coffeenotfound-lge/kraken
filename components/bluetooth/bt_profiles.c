#include "kraken/bt_profiles.h"
#include "kraken/kernel.h"
#include "esp_log.h"
#include <string.h>

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_spp_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"
#include "esp_hf_client_api.h"
#include "esp_hidh_api.h"
#include "esp_hidd_api.h"
#endif

static const char *TAG = "bt_profiles";

#if CONFIG_BT_ENABLED

static struct {
    bool initialized;
    uint32_t enabled_profiles;  // Bitmask of enabled profiles
    
    // SPP
    uint32_t spp_handle;
    bt_spp_data_cb_t spp_data_cb;
    void *spp_user_data;
    
    // A2DP
    bool a2dp_playing;
    bt_a2dp_data_cb_t a2dp_data_cb;
    void *a2dp_user_data;
    
    // AVRCP
    uint8_t avrcp_tl;  // Transaction label
    
    // HFP
    bool hfp_connected;
    
    // HID
    bt_hid_data_cb_t hid_data_cb;
    void *hid_user_data;
    
    // Status callback
    bt_profile_status_cb_t status_cb;
    void *status_user_data;
} g_profiles = {0};

// Helper to check if profile is enabled
static bool is_profile_enabled(bt_profile_t profile)
{
    return (g_profiles.enabled_profiles & (1 << profile)) != 0;
}

static void set_profile_enabled(bt_profile_t profile, bool enabled)
{
    if (enabled) {
        g_profiles.enabled_profiles |= (1 << profile);
    } else {
        g_profiles.enabled_profiles &= ~(1 << profile);
    }
}

// ============================================================================
// SPP (Serial Port Profile) Implementation
// ============================================================================

static void spp_callback(esp_spp_cb_event_t event, esp_spp_cb_param_t *param)
{
    switch (event) {
        case ESP_SPP_INIT_EVT:
            ESP_LOGI(TAG, "SPP initialized");
            esp_spp_start_srv(ESP_SPP_SEC_NONE, ESP_SPP_ROLE_SLAVE, 0, "SPP_SERVER");
            break;
            
        case ESP_SPP_SRV_OPEN_EVT:
            ESP_LOGI(TAG, "SPP connection opened, handle: %d", param->srv_open.handle);
            g_profiles.spp_handle = param->srv_open.handle;
            if (g_profiles.status_cb) {
                g_profiles.status_cb(BT_PROFILE_SPP, true, g_profiles.status_user_data);
            }
            kraken_event_post(KRAKEN_EVENT_BT_CONNECTED, NULL, 0);
            break;
            
        case ESP_SPP_CLOSE_EVT:
            ESP_LOGI(TAG, "SPP connection closed");
            g_profiles.spp_handle = 0;
            if (g_profiles.status_cb) {
                g_profiles.status_cb(BT_PROFILE_SPP, false, g_profiles.status_user_data);
            }
            kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            break;
            
        case ESP_SPP_DATA_IND_EVT:
            ESP_LOGI(TAG, "SPP data received: %d bytes", param->data_ind.len);
            if (g_profiles.spp_data_cb) {
                g_profiles.spp_data_cb(param->data_ind.data, param->data_ind.len, 
                                       g_profiles.spp_user_data);
            }
            break;
            
        case ESP_SPP_WRITE_EVT:
            ESP_LOGD(TAG, "SPP write complete");
            break;
            
        default:
            ESP_LOGD(TAG, "SPP event: %d", event);
            break;
    }
}

static esp_err_t spp_init(void)
{
    esp_err_t ret = esp_spp_register_callback(spp_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPP register callback failed: %d", ret);
        return ret;
    }
    
    ret = esp_spp_init(ESP_SPP_MODE_CB);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPP init failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "SPP initialized");
    return ESP_OK;
}

static esp_err_t spp_deinit(void)
{
    esp_spp_deinit();
    ESP_LOGI(TAG, "SPP deinitialized");
    return ESP_OK;
}


// ============================================================================
// A2DP (Advanced Audio Distribution Profile) Implementation
// ============================================================================

static void a2dp_callback(esp_a2d_cb_event_t event, esp_a2d_cb_param_t *param)
{
    switch (event) {
        case ESP_A2D_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "A2DP connected");
                if (g_profiles.status_cb) {
                    g_profiles.status_cb(BT_PROFILE_A2DP_SINK, true, g_profiles.status_user_data);
                }
                kraken_event_post(KRAKEN_EVENT_BT_CONNECTED, NULL, 0);
            } else if (param->conn_stat.state == ESP_A2D_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "A2DP disconnected");
                g_profiles.a2dp_playing = false;
                if (g_profiles.status_cb) {
                    g_profiles.status_cb(BT_PROFILE_A2DP_SINK, false, g_profiles.status_user_data);
                }
                kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            }
            break;
            
        case ESP_A2D_AUDIO_STATE_EVT:
            if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STARTED) {
                ESP_LOGI(TAG, "A2DP audio started");
                g_profiles.a2dp_playing = true;
            } else if (param->audio_stat.state == ESP_A2D_AUDIO_STATE_STOPPED) {
                ESP_LOGI(TAG, "A2DP audio stopped");
                g_profiles.a2dp_playing = false;
            }
            break;
            
        case ESP_A2D_AUDIO_CFG_EVT:
            ESP_LOGI(TAG, "A2DP audio config, codec type: %d", param->audio_cfg.mcc.type);
            break;
            
        default:
            ESP_LOGD(TAG, "A2DP event: %d", event);
            break;
    }
}

static void a2dp_data_callback(const uint8_t *data, uint32_t len)
{
    if (g_profiles.a2dp_data_cb) {
        g_profiles.a2dp_data_cb(data, len, g_profiles.a2dp_user_data);
    }
}

static esp_err_t a2dp_sink_init(void)
{
    esp_err_t ret = esp_a2d_register_callback(a2dp_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP register callback failed: %d", ret);
        return ret;
    }
    
    ret = esp_a2d_sink_register_data_callback(a2dp_data_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP register data callback failed: %d", ret);
        return ret;
    }
    
    ret = esp_a2d_sink_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "A2DP sink init failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "A2DP Sink initialized");
    return ESP_OK;
}

static esp_err_t a2dp_sink_deinit(void)
{
    esp_a2d_sink_deinit();
    ESP_LOGI(TAG, "A2DP Sink deinitialized");
    return ESP_OK;
}


// ============================================================================
// AVRCP (Audio/Video Remote Control Profile) Implementation
// ============================================================================

static void avrcp_callback(esp_avrc_ct_cb_event_t event, esp_avrc_ct_cb_param_t *param)
{
    switch (event) {
        case ESP_AVRC_CT_CONNECTION_STATE_EVT:
            if (param->conn_stat.connected) {
                ESP_LOGI(TAG, "AVRCP connected");
                if (g_profiles.status_cb) {
                    g_profiles.status_cb(BT_PROFILE_AVRCP, true, g_profiles.status_user_data);
                }
            } else {
                ESP_LOGI(TAG, "AVRCP disconnected");
                if (g_profiles.status_cb) {
                    g_profiles.status_cb(BT_PROFILE_AVRCP, false, g_profiles.status_user_data);
                }
            }
            break;
            
        case ESP_AVRC_CT_PASSTHROUGH_RSP_EVT:
            ESP_LOGI(TAG, "AVRCP passthrough response, key: 0x%x", param->psth_rsp.key_code);
            break;
            
        case ESP_AVRC_CT_METADATA_RSP_EVT:
            ESP_LOGI(TAG, "AVRCP metadata response");
            break;
            
        case ESP_AVRC_CT_CHANGE_NOTIFY_EVT:
            ESP_LOGI(TAG, "AVRCP change notify, event: %d", param->change_ntf.event_id);
            break;
            
        case ESP_AVRC_CT_REMOTE_FEATURES_EVT:
            ESP_LOGI(TAG, "AVRCP remote features: 0x%x", param->rmt_feats.feat_mask);
            break;
            
        default:
            ESP_LOGD(TAG, "AVRCP event: %d", event);
            break;
    }
}

static esp_err_t avrcp_init(void)
{
    esp_err_t ret = esp_avrc_ct_register_callback(avrcp_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AVRCP register callback failed: %d", ret);
        return ret;
    }
    
    ret = esp_avrc_ct_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AVRCP init failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "AVRCP initialized");
    return ESP_OK;
}

static esp_err_t avrcp_deinit(void)
{
    esp_avrc_ct_deinit();
    ESP_LOGI(TAG, "AVRCP deinitialized");
    return ESP_OK;
}


// ============================================================================
// HFP Client (Hands-Free Profile) Implementation
// ============================================================================

static void hfp_callback(esp_hf_client_cb_event_t event, esp_hf_client_cb_param_t *param)
{
    switch (event) {
        case ESP_HF_CLIENT_CONNECTION_STATE_EVT:
            if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_CONNECTED) {
                ESP_LOGI(TAG, "HFP connected");
                g_profiles.hfp_connected = true;
                if (g_profiles.status_cb) {
                    g_profiles.status_cb(BT_PROFILE_HFP_CLIENT, true, g_profiles.status_user_data);
                }
                kraken_event_post(KRAKEN_EVENT_BT_CONNECTED, NULL, 0);
            } else if (param->conn_stat.state == ESP_HF_CLIENT_CONNECTION_STATE_DISCONNECTED) {
                ESP_LOGI(TAG, "HFP disconnected");
                g_profiles.hfp_connected = false;
                if (g_profiles.status_cb) {
                    g_profiles.status_cb(BT_PROFILE_HFP_CLIENT, false, g_profiles.status_user_data);
                }
                kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            }
            break;
            
        case ESP_HF_CLIENT_AUDIO_STATE_EVT:
            ESP_LOGI(TAG, "HFP audio state: %d", param->audio_stat.state);
            break;
            
        case ESP_HF_CLIENT_VOLUME_CONTROL_EVT:
            ESP_LOGI(TAG, "HFP volume: %d", param->volume_control.volume);
            break;
            
        default:
            ESP_LOGD(TAG, "HFP event: %d", event);
            break;
    }
}

static esp_err_t hfp_client_init(void)
{
    esp_err_t ret = esp_hf_client_register_callback(hfp_callback);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HFP register callback failed: %d", ret);
        return ret;
    }
    
    ret = esp_hf_client_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HFP init failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "HFP Client initialized");
    return ESP_OK;
}

static esp_err_t hfp_client_deinit(void)
{
    esp_hf_client_deinit();
    ESP_LOGI(TAG, "HFP Client deinitialized");
    return ESP_OK;
}

// ============================================================================
// HID Host Implementation
// ============================================================================

static void hidh_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidh_event_t event = (esp_hidh_event_t)id;
    esp_hidh_event_data_t *param = (esp_hidh_event_data_t *)event_data;
    
    switch (event) {
        case ESP_HIDH_OPEN_EVENT:
            if (param->open.status == ESP_OK) {
                ESP_LOGI(TAG, "HID Host device opened");
                if (g_profiles.status_cb) {
                    g_profiles.status_cb(BT_PROFILE_HID_HOST, true, g_profiles.status_user_data);
                }
                kraken_event_post(KRAKEN_EVENT_BT_CONNECTED, NULL, 0);
            }
            break;
            
        case ESP_HIDH_CLOSE_EVENT:
            ESP_LOGI(TAG, "HID Host device closed");
            if (g_profiles.status_cb) {
                g_profiles.status_cb(BT_PROFILE_HID_HOST, false, g_profiles.status_user_data);
            }
            kraken_event_post(KRAKEN_EVENT_BT_DISCONNECTED, NULL, 0);
            break;
            
        case ESP_HIDH_INPUT_EVENT:
            ESP_LOGD(TAG, "HID input: len=%d", param->input.length);
            if (g_profiles.hid_data_cb) {
                g_profiles.hid_data_cb(param->input.report_type, 
                                      param->input.data, 
                                      param->input.length,
                                      g_profiles.hid_user_data);
            }
            break;
            
        default:
            ESP_LOGD(TAG, "HID Host event: %d", event);
            break;
    }
}

static esp_err_t hidh_init(void)
{
    esp_hidh_config_t config = {
        .callback = hidh_callback,
        .event_stack_size = 4096,
    };
    
    esp_err_t ret = esp_hidh_init(&config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HID Host init failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "HID Host initialized");
    return ESP_OK;
}

static esp_err_t hidh_deinit(void)
{
    esp_hidh_deinit();
    ESP_LOGI(TAG, "HID Host deinitialized");
    return ESP_OK;
}


// ============================================================================
// Public API Implementation
// ============================================================================

esp_err_t bt_profiles_init(void)
{
    if (g_profiles.initialized) {
        return ESP_OK;
    }
    
    memset(&g_profiles, 0, sizeof(g_profiles));
    g_profiles.initialized = true;
    g_profiles.avrcp_tl = 0;
    
    ESP_LOGI(TAG, "Bluetooth profiles initialized");
    return ESP_OK;
}

esp_err_t bt_profiles_deinit(void)
{
    if (!g_profiles.initialized) {
        return ESP_OK;
    }
    
    // Disable all enabled profiles
    if (is_profile_enabled(BT_PROFILE_SPP)) {
        spp_deinit();
    }
    if (is_profile_enabled(BT_PROFILE_A2DP_SINK)) {
        a2dp_sink_deinit();
    }
    if (is_profile_enabled(BT_PROFILE_AVRCP)) {
        avrcp_deinit();
    }
    if (is_profile_enabled(BT_PROFILE_HFP_CLIENT)) {
        hfp_client_deinit();
    }
    if (is_profile_enabled(BT_PROFILE_HID_HOST)) {
        hidh_deinit();
    }
    
    g_profiles.initialized = false;
    ESP_LOGI(TAG, "Bluetooth profiles deinitialized");
    return ESP_OK;
}

esp_err_t bt_profile_enable(bt_profile_t profile)
{
    if (!g_profiles.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (is_profile_enabled(profile)) {
        return ESP_OK;  // Already enabled
    }
    
    esp_err_t ret = ESP_OK;
    
    switch (profile) {
        case BT_PROFILE_SPP:
            ret = spp_init();
            break;
            
        case BT_PROFILE_A2DP_SINK:
            ret = a2dp_sink_init();
            break;
            
        case BT_PROFILE_AVRCP:
            ret = avrcp_init();
            break;
            
        case BT_PROFILE_HFP_CLIENT:
            ret = hfp_client_init();
            break;
            
        case BT_PROFILE_HID_HOST:
            ret = hidh_init();
            break;
            
        default:
            ESP_LOGE(TAG, "Unsupported profile: %d", profile);
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    if (ret == ESP_OK) {
        set_profile_enabled(profile, true);
        ESP_LOGI(TAG, "Profile %d enabled", profile);
    } else {
        ESP_LOGE(TAG, "Failed to enable profile %d: %d", profile, ret);
    }
    
    return ret;
}

esp_err_t bt_profile_disable(bt_profile_t profile)
{
    if (!g_profiles.initialized) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (!is_profile_enabled(profile)) {
        return ESP_OK;  // Already disabled
    }
    
    esp_err_t ret = ESP_OK;
    
    switch (profile) {
        case BT_PROFILE_SPP:
            ret = spp_deinit();
            break;
            
        case BT_PROFILE_A2DP_SINK:
            ret = a2dp_sink_deinit();
            break;
            
        case BT_PROFILE_AVRCP:
            ret = avrcp_deinit();
            break;
            
        case BT_PROFILE_HFP_CLIENT:
            ret = hfp_client_deinit();
            break;
            
        case BT_PROFILE_HID_HOST:
            ret = hidh_deinit();
            break;
            
        default:
            return ESP_ERR_NOT_SUPPORTED;
    }
    
    if (ret == ESP_OK) {
        set_profile_enabled(profile, false);
        ESP_LOGI(TAG, "Profile %d disabled", profile);
    }
    
    return ret;
}

bool bt_profile_is_enabled(bt_profile_t profile)
{
    return is_profile_enabled(profile);
}

void bt_profile_set_status_callback(bt_profile_status_cb_t cb, void *user_data)
{
    g_profiles.status_cb = cb;
    g_profiles.status_user_data = user_data;
}

// SPP Functions
esp_err_t bt_spp_write(const uint8_t *data, uint16_t len)
{
    if (!is_profile_enabled(BT_PROFILE_SPP) || g_profiles.spp_handle == 0) {
        return ESP_ERR_INVALID_STATE;
    }
    
    return esp_spp_write(g_profiles.spp_handle, len, (uint8_t *)data);
}

void bt_spp_set_data_callback(bt_spp_data_cb_t cb, void *user_data)
{
    g_profiles.spp_data_cb = cb;
    g_profiles.spp_user_data = user_data;
}

// A2DP Functions
esp_err_t bt_a2dp_start_playback(void)
{
    if (!is_profile_enabled(BT_PROFILE_A2DP_SINK)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    // A2DP playback is automatically started when source starts streaming
    ESP_LOGI(TAG, "A2DP playback start requested");
    return ESP_OK;
}

esp_err_t bt_a2dp_stop_playback(void)
{
    if (!is_profile_enabled(BT_PROFILE_A2DP_SINK)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "A2DP playback stop requested");
    return ESP_OK;
}

void bt_a2dp_set_data_callback(bt_a2dp_data_cb_t cb, void *user_data)
{
    g_profiles.a2dp_data_cb = cb;
    g_profiles.a2dp_user_data = user_data;
}


// AVRCP Functions
esp_err_t bt_avrcp_send_play(void)
{
    if (!is_profile_enabled(BT_PROFILE_AVRCP)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                                       ESP_AVRC_PT_CMD_PLAY, 
                                                       ESP_AVRC_PT_CMD_STATE_PRESSED);
    if (ret == ESP_OK) {
        esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                         ESP_AVRC_PT_CMD_PLAY, 
                                         ESP_AVRC_PT_CMD_STATE_RELEASED);
    }
    return ret;
}

esp_err_t bt_avrcp_send_pause(void)
{
    if (!is_profile_enabled(BT_PROFILE_AVRCP)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                                       ESP_AVRC_PT_CMD_PAUSE, 
                                                       ESP_AVRC_PT_CMD_STATE_PRESSED);
    if (ret == ESP_OK) {
        esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                         ESP_AVRC_PT_CMD_PAUSE, 
                                         ESP_AVRC_PT_CMD_STATE_RELEASED);
    }
    return ret;
}

esp_err_t bt_avrcp_send_next(void)
{
    if (!is_profile_enabled(BT_PROFILE_AVRCP)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                                       ESP_AVRC_PT_CMD_FORWARD, 
                                                       ESP_AVRC_PT_CMD_STATE_PRESSED);
    if (ret == ESP_OK) {
        esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                         ESP_AVRC_PT_CMD_FORWARD, 
                                         ESP_AVRC_PT_CMD_STATE_RELEASED);
    }
    return ret;
}

esp_err_t bt_avrcp_send_prev(void)
{
    if (!is_profile_enabled(BT_PROFILE_AVRCP)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                                       ESP_AVRC_PT_CMD_BACKWARD, 
                                                       ESP_AVRC_PT_CMD_STATE_PRESSED);
    if (ret == ESP_OK) {
        esp_avrc_ct_send_passthrough_cmd(g_profiles.avrcp_tl++, 
                                         ESP_AVRC_PT_CMD_BACKWARD, 
                                         ESP_AVRC_PT_CMD_STATE_RELEASED);
    }
    return ret;
}

esp_err_t bt_avrcp_set_volume(uint8_t volume)
{
    if (!is_profile_enabled(BT_PROFILE_AVRCP)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Setting AVRCP volume to %d", volume);
    return ESP_OK;
}

// HID Functions
esp_err_t bt_hid_send_keyboard(uint8_t modifier, uint8_t *keycodes, uint8_t keycode_count)
{
    if (!is_profile_enabled(BT_PROFILE_HID_DEVICE)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t report[8] = {0};
    report[0] = modifier;
    
    for (int i = 0; i < keycode_count && i < 6; i++) {
        report[i + 2] = keycodes[i];
    }
    
    ESP_LOGI(TAG, "Sending keyboard report");
    return ESP_OK;
}

esp_err_t bt_hid_send_mouse(int8_t x, int8_t y, uint8_t buttons)
{
    if (!is_profile_enabled(BT_PROFILE_HID_DEVICE)) {
        return ESP_ERR_INVALID_STATE;
    }
    
    uint8_t report[4];
    report[0] = buttons;
    report[1] = (uint8_t)x;
    report[2] = (uint8_t)y;
    report[3] = 0;
    
    ESP_LOGI(TAG, "Sending mouse report: x=%d, y=%d, buttons=0x%02x", x, y, buttons);
    return ESP_OK;
}

void bt_hid_set_data_callback(bt_hid_data_cb_t cb, void *user_data)
{
    g_profiles.hid_data_cb = cb;
    g_profiles.hid_user_data = user_data;
}

#else // !CONFIG_BT_ENABLED

// Stub implementations when Bluetooth is disabled
esp_err_t bt_profiles_init(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_profiles_deinit(void) { return ESP_OK; }
esp_err_t bt_profile_enable(bt_profile_t profile) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_profile_disable(bt_profile_t profile) { return ESP_OK; }
bool bt_profile_is_enabled(bt_profile_t profile) { return false; }
void bt_profile_set_status_callback(bt_profile_status_cb_t cb, void *user_data) {}

esp_err_t bt_spp_write(const uint8_t *data, uint16_t len) { return ESP_ERR_NOT_SUPPORTED; }
void bt_spp_set_data_callback(bt_spp_data_cb_t cb, void *user_data) {}

esp_err_t bt_a2dp_start_playback(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_a2dp_stop_playback(void) { return ESP_ERR_NOT_SUPPORTED; }
void bt_a2dp_set_data_callback(bt_a2dp_data_cb_t cb, void *user_data) {}

esp_err_t bt_avrcp_send_play(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_avrcp_send_pause(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_avrcp_send_next(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_avrcp_send_prev(void) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_avrcp_set_volume(uint8_t volume) { return ESP_ERR_NOT_SUPPORTED; }

esp_err_t bt_hid_send_keyboard(uint8_t modifier, uint8_t *keycodes, uint8_t keycode_count) { return ESP_ERR_NOT_SUPPORTED; }
esp_err_t bt_hid_send_mouse(int8_t x, int8_t y, uint8_t buttons) { return ESP_ERR_NOT_SUPPORTED; }
void bt_hid_set_data_callback(bt_hid_data_cb_t cb, void *user_data) {}

#endif // CONFIG_BT_ENABLED
