#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KRAKEN_SERVICE_NAME_MAX_LEN 32
#define KRAKEN_MAX_SERVICES 16
#define KRAKEN_MAX_EVENT_LISTENERS 32

typedef enum {
    KRAKEN_OK = 0,
    KRAKEN_ERR_NO_MEM = -1,
    KRAKEN_ERR_INVALID_ARG = -2,
    KRAKEN_ERR_NOT_FOUND = -3,
    KRAKEN_ERR_ALREADY_EXISTS = -4,
    KRAKEN_ERR_PERMISSION_DENIED = -5,
    KRAKEN_ERR_NOT_INITIALIZED = -6,
    KRAKEN_ERR_TIMEOUT = -7,
    KRAKEN_ERR_FAIL = -8,
} kraken_err_t;

typedef enum {
    KRAKEN_EVENT_NONE = 0,
    
    KRAKEN_EVENT_WIFI_SCAN_DONE = 100,
    KRAKEN_EVENT_WIFI_CONNECTED,
    KRAKEN_EVENT_WIFI_DISCONNECTED,
    KRAKEN_EVENT_WIFI_GOT_IP,
    
    KRAKEN_EVENT_BT_SCAN_DONE = 200,
    KRAKEN_EVENT_BT_CONNECTED,
    KRAKEN_EVENT_BT_DISCONNECTED,
    
    KRAKEN_EVENT_INPUT_UP = 300,
    KRAKEN_EVENT_INPUT_DOWN,
    KRAKEN_EVENT_INPUT_LEFT,
    KRAKEN_EVENT_INPUT_RIGHT,
    KRAKEN_EVENT_INPUT_CENTER,
    
    KRAKEN_EVENT_DISPLAY_REFRESH = 400,
    KRAKEN_EVENT_DISPLAY_TOUCH,
    
    KRAKEN_EVENT_AUDIO_PLAY_DONE = 500,
    KRAKEN_EVENT_AUDIO_RECORD_DONE,
    
    KRAKEN_EVENT_SYSTEM_TIME_SYNC = 600,
    KRAKEN_EVENT_SYSTEM_LOW_MEMORY,
    KRAKEN_EVENT_SYSTEM_WATCHDOG,
    
    KRAKEN_EVENT_APP_INSTALLED = 700,
    KRAKEN_EVENT_APP_UNINSTALLED,
    KRAKEN_EVENT_APP_STARTED,
    KRAKEN_EVENT_APP_STOPPED,
    
    KRAKEN_EVENT_USER_CUSTOM = 1000,
} kraken_event_type_t;

typedef enum {
    KRAKEN_PERM_NONE = 0,
    KRAKEN_PERM_WIFI = (1 << 0),
    KRAKEN_PERM_BT = (1 << 1),
    KRAKEN_PERM_DISPLAY = (1 << 2),
    KRAKEN_PERM_AUDIO = (1 << 3),
    KRAKEN_PERM_STORAGE = (1 << 4),
    KRAKEN_PERM_NETWORK = (1 << 5),
    KRAKEN_PERM_SYSTEM = (1 << 6),
    KRAKEN_PERM_ALL = 0xFFFFFFFF,
} kraken_permission_t;

typedef struct {
    kraken_event_type_t type;
    void *data;
    uint32_t data_len;
    uint32_t timestamp;
} kraken_event_t;

typedef void (*kraken_event_handler_t)(const kraken_event_t *event, void *user_data);

// Forward declaration - internal structure not exposed
typedef struct kraken_service_t kraken_service_t;

esp_err_t kraken_kernel_init(void);
esp_err_t kraken_kernel_deinit(void);

// Service management
esp_err_t kraken_service_register(const char *name, uint32_t permissions,
                                   esp_err_t (*init_fn)(void),
                                   esp_err_t (*deinit_fn)(void));
esp_err_t kraken_service_unregister(const char *name);
esp_err_t kraken_service_start(const char *name);
esp_err_t kraken_service_stop(const char *name);

// Permission checking
bool kraken_service_has_permission(const char *name, kraken_permission_t perm);
esp_err_t kraken_check_caller_permission(kraken_permission_t required_perm);

// Helper macro to check permissions in service APIs
#define KRAKEN_CHECK_PERMISSION(perm) \
    do { \
        esp_err_t _ret = kraken_check_caller_permission(perm); \
        if (_ret != ESP_OK) { \
            ESP_LOGE("PERMISSION", "Permission denied: required 0x%lx", (uint32_t)(perm)); \
            return ESP_ERR_INVALID_STATE; \
        } \
    } while(0)

esp_err_t kraken_event_subscribe(kraken_event_type_t event_type,
                                  kraken_event_handler_t handler,
                                  void *user_data);
esp_err_t kraken_event_unsubscribe(kraken_event_type_t event_type,
                                    kraken_event_handler_t handler);
esp_err_t kraken_event_post(kraken_event_type_t event_type, 
                             void *data, uint32_t data_len);
esp_err_t kraken_event_post_from_isr(kraken_event_type_t event_type,
                                      void *data, uint32_t data_len);

void *kraken_malloc(size_t size);
void *kraken_calloc(size_t nmemb, size_t size);
void *kraken_realloc(void *ptr, size_t size);
void kraken_free(void *ptr);
size_t kraken_get_free_heap_size(void);
size_t kraken_get_minimum_free_heap_size(void);

esp_err_t kraken_timer_create(const char *name, uint32_t period_ms,
                               bool auto_reload, void (*callback)(void*),
                               void *arg, void **handle);
esp_err_t kraken_timer_start(void *handle);
esp_err_t kraken_timer_stop(void *handle);
esp_err_t kraken_timer_delete(void *handle);

uint32_t kraken_get_tick_count(void);
void kraken_delay_ms(uint32_t ms);

#ifdef __cplusplus
}
#endif
