#pragma once

#include "kraken/kernel.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/task.h"

#define KRAKEN_TLS_INDEX 0  // Thread-local storage index for current service

// Full service structure - kept internal to prevent permission tampering
struct kraken_service_t {
    char name[KRAKEN_SERVICE_NAME_MAX_LEN];
    uint32_t permissions;  // Immutable after registration
    esp_err_t (*init)(void);
    esp_err_t (*deinit)(void);
    bool is_running;
    void *priv_data;
    uint32_t perm_checksum;  // Checksum to detect permission tampering
};

typedef struct {
    kraken_event_type_t event_type;
    kraken_event_handler_t handler;
    void *user_data;
} event_listener_t;

typedef struct {
    bool initialized;
    SemaphoreHandle_t service_mutex;
    SemaphoreHandle_t event_mutex;
    QueueHandle_t event_queue;
    TaskHandle_t event_task;
    kraken_service_t services[KRAKEN_MAX_SERVICES];
    uint8_t service_count;
    event_listener_t listeners[KRAKEN_MAX_EVENT_LISTENERS];
    uint8_t listener_count;
} kernel_state_t;

extern kernel_state_t g_kernel;

// Service management functions
esp_err_t kernel_service_init(void);
void kernel_service_cleanup(void);

// Permission helpers
void kernel_set_current_service(const char *service_name);
const char* kernel_get_current_service(void);
kraken_service_t* kernel_find_service(const char *name);
uint32_t kernel_calculate_perm_checksum(const char *name, uint32_t permissions);
bool kernel_verify_permissions(kraken_service_t *svc);

// Event system functions  
esp_err_t kernel_event_init(void);
void kernel_event_cleanup(void);
void kernel_event_task(void *arg);
