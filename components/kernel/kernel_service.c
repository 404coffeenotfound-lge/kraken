#include "kernel_internal.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "kernel_svc";

// Simple checksum to detect permission tampering
// Uses name and permissions to create a verification hash
uint32_t kernel_calculate_perm_checksum(const char *name, uint32_t permissions)
{
    uint32_t hash = 0x811C9DC5;  // FNV-1a offset basis
    
    // Hash the service name
    for (const char *p = name; *p != '\0'; p++) {
        hash ^= (uint32_t)*p;
        hash *= 0x01000193;  // FNV-1a prime
    }
    
    // Mix in the permissions
    hash ^= permissions;
    hash *= 0x01000193;
    
    // Add a secret constant (makes it harder to forge)
    hash ^= 0xDEADBEEF;
    hash *= 0x01000193;
    
    return hash;
}

bool kernel_verify_permissions(kraken_service_t *svc)
{
    if (!svc) return false;
    
    uint32_t expected = kernel_calculate_perm_checksum(svc->name, svc->permissions);
    if (svc->perm_checksum != expected) {
        ESP_LOGE(TAG, "SECURITY: Permission tampering detected for service '%s'!", svc->name);
        ESP_LOGE(TAG, "Expected checksum: 0x%08lx, Got: 0x%08lx", expected, svc->perm_checksum);
        return false;
    }
    return true;
}

// Thread-local storage helpers
void kernel_set_current_service(const char *service_name)
{
    vTaskSetThreadLocalStoragePointer(NULL, KRAKEN_TLS_INDEX, (void*)service_name);
}

const char* kernel_get_current_service(void)
{
    return (const char*)pvTaskGetThreadLocalStoragePointer(NULL, KRAKEN_TLS_INDEX);
}

kraken_service_t* kernel_find_service(const char *name)
{
    if (!name) return NULL;
    
    for (uint8_t i = 0; i < g_kernel.service_count; i++) {
        if (strcmp(g_kernel.services[i].name, name) == 0) {
            return &g_kernel.services[i];
        }
    }
    return NULL;
}

esp_err_t kernel_service_init(void)
{
    g_kernel.service_mutex = xSemaphoreCreateMutex();
    if (!g_kernel.service_mutex) {
        ESP_LOGE(TAG, "Failed to create service mutex");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void kernel_service_cleanup(void)
{
    if (g_kernel.service_mutex) {
        vSemaphoreDelete(g_kernel.service_mutex);
        g_kernel.service_mutex = NULL;
    }
}

esp_err_t kraken_service_register(const char *name, uint32_t permissions,
                                   esp_err_t (*init_fn)(void),
                                   esp_err_t (*deinit_fn)(void))
{
    if (!g_kernel.initialized || !name) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_kernel.service_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    if (g_kernel.service_count >= KRAKEN_MAX_SERVICES) {
        xSemaphoreGive(g_kernel.service_mutex);
        ESP_LOGE(TAG, "Max services reached");
        return ESP_ERR_NO_MEM;
    }

    for (uint8_t i = 0; i < g_kernel.service_count; i++) {
        if (strcmp(g_kernel.services[i].name, name) == 0) {
            xSemaphoreGive(g_kernel.service_mutex);
            ESP_LOGE(TAG, "Service %s already exists", name);
            return ESP_ERR_INVALID_STATE;
        }
    }

    kraken_service_t *svc = &g_kernel.services[g_kernel.service_count];
    strncpy(svc->name, name, KRAKEN_SERVICE_NAME_MAX_LEN - 1);
    svc->name[KRAKEN_SERVICE_NAME_MAX_LEN - 1] = '\0';
    svc->permissions = permissions;
    svc->init = init_fn;
    svc->deinit = deinit_fn;
    svc->is_running = false;
    svc->priv_data = NULL;
    
    // Calculate and store checksum to detect permission tampering
    svc->perm_checksum = kernel_calculate_perm_checksum(name, permissions);

    g_kernel.service_count++;
    xSemaphoreGive(g_kernel.service_mutex);

    ESP_LOGI(TAG, "Service '%s' registered with permissions 0x%lx", name, permissions);
    return ESP_OK;
}

esp_err_t kraken_service_unregister(const char *name)
{
    if (!g_kernel.initialized || !name) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_kernel.service_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    for (uint8_t i = 0; i < g_kernel.service_count; i++) {
        if (strcmp(g_kernel.services[i].name, name) == 0) {
            if (g_kernel.services[i].is_running) {
                if (g_kernel.services[i].deinit) {
                    g_kernel.services[i].deinit();
                }
                g_kernel.services[i].is_running = false;
            }
            
            for (uint8_t j = i; j < g_kernel.service_count - 1; j++) {
                g_kernel.services[j] = g_kernel.services[j + 1];
            }
            g_kernel.service_count--;
            xSemaphoreGive(g_kernel.service_mutex);
            ESP_LOGI(TAG, "Service '%s' unregistered", name);
            return ESP_OK;
        }
    }

    xSemaphoreGive(g_kernel.service_mutex);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t kraken_service_start(const char *name)
{
    if (!g_kernel.initialized || !name) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_kernel.service_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    kraken_service_t *svc = kernel_find_service(name);
    if (!svc) {
        xSemaphoreGive(g_kernel.service_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (svc->is_running) {
        xSemaphoreGive(g_kernel.service_mutex);
        return ESP_OK;
    }

    if (svc->init) {
        // Set service context before calling init
        kernel_set_current_service(name);
        esp_err_t ret = svc->init();
        kernel_set_current_service(NULL);
        
        if (ret != ESP_OK) {
            xSemaphoreGive(g_kernel.service_mutex);
            ESP_LOGE(TAG, "Failed to initialize service '%s': %d", name, ret);
            return ret;
        }
    }

    svc->is_running = true;
    xSemaphoreGive(g_kernel.service_mutex);
    ESP_LOGI(TAG, "Service '%s' started", name);
    return ESP_OK;
}

esp_err_t kraken_service_stop(const char *name)
{
    if (!g_kernel.initialized || !name) {
        return ESP_ERR_INVALID_ARG;
    }

    if (xSemaphoreTake(g_kernel.service_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }

    kraken_service_t *svc = kernel_find_service(name);
    if (!svc) {
        xSemaphoreGive(g_kernel.service_mutex);
        return ESP_ERR_NOT_FOUND;
    }

    if (!svc->is_running) {
        xSemaphoreGive(g_kernel.service_mutex);
        return ESP_OK;
    }

    if (svc->deinit) {
        // Set service context before calling deinit
        kernel_set_current_service(name);
        esp_err_t ret = svc->deinit();
        kernel_set_current_service(NULL);
        
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Service '%s' deinit failed: %d", name, ret);
        }
    }

    svc->is_running = false;
    xSemaphoreGive(g_kernel.service_mutex);
    ESP_LOGI(TAG, "Service '%s' stopped", name);
    return ESP_OK;
}

bool kraken_service_has_permission(const char *name, kraken_permission_t perm)
{
    if (!g_kernel.initialized || !name) {
        return false;
    }

    bool has_perm = false;
    if (xSemaphoreTake(g_kernel.service_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        kraken_service_t *svc = kernel_find_service(name);
        if (svc) {
            // Verify permissions haven't been tampered with
            if (kernel_verify_permissions(svc)) {
                has_perm = (svc->permissions & perm) != 0;
            } else {
                ESP_LOGE(TAG, "Permission verification failed for service '%s'", name);
            }
        }
        xSemaphoreGive(g_kernel.service_mutex);
    }

    return has_perm;
}

esp_err_t kraken_check_caller_permission(kraken_permission_t required_perm)
{
    if (!g_kernel.initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    // Get the current service name from thread-local storage
    const char *caller = kernel_get_current_service();
    if (!caller) {
        // No service context set - could be system/kernel call
        ESP_LOGW(TAG, "No service context for permission check");
        return ESP_ERR_INVALID_STATE;
    }

    // Find the service and verify integrity
    kraken_service_t *svc = kernel_find_service(caller);
    if (!svc) {
        ESP_LOGE(TAG, "Service '%s' not found", caller);
        return ESP_ERR_NOT_FOUND;
    }

    // Verify permissions haven't been tampered with
    if (!kernel_verify_permissions(svc)) {
        ESP_LOGE(TAG, "SECURITY VIOLATION: Service '%s' permissions tampered!", caller);
        return ESP_ERR_INVALID_STATE;
    }

    // Check if the caller has the required permission
    if ((svc->permissions & required_perm) == 0) {
        ESP_LOGE(TAG, "Service '%s' denied: missing permission 0x%lx", 
                 caller, (uint32_t)required_perm);
        return ESP_ERR_NOT_ALLOWED;
    }

    return ESP_OK;
}
