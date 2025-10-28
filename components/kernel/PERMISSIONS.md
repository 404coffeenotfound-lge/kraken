# Kraken Permission System Documentation

## Overview

The Kraken kernel implements a **secure**, permission-based access control system that prevents unauthorized services from calling protected APIs. Each service is registered with a set of permissions that **cannot be modified at runtime**.

## Security Features

### 1. Immutable Permissions
- Permissions are set once during service registration
- The service structure is kept **internal** (not exposed in public headers)
- Direct access to the permissions field is prevented by design

### 2. Tamper Detection
- Each service has a cryptographic checksum of its permissions
- Checksum is calculated using: `hash(service_name + permissions + secret)`
- Every permission check verifies the checksum
- **Any tampering attempt is detected and logged as a security violation**

### 3. Runtime Protection
```c
// If someone tries to modify permissions in memory:
my_service->permissions = KRAKEN_PERM_ALL;  // Won't work - structure is private!

// Even if they somehow modify it, the checksum verification will fail:
// E (12345) kernel_svc: SECURITY: Permission tampering detected for service 'my_app'!
// E (12346) kernel_svc: Expected checksum: 0x12345678, Got: 0xABCDEF00
```

## How It Works

### 1. Service Registration with Permissions

When registering a service, you specify which permissions it has:

```c
// Register a service with WiFi and Network permissions
kraken_service_register("my_app", 
                       KRAKEN_PERM_WIFI | KRAKEN_PERM_NETWORK,
                       my_app_init,
                       my_app_deinit);
```

### 2. API Permission Checks

Protected APIs use the `KRAKEN_CHECK_PERMISSION` macro to verify caller permissions:

```c
esp_err_t wifi_service_connect(const char *ssid, const char *password)
{
    // This will fail if the calling service doesn't have KRAKEN_PERM_WIFI
    KRAKEN_CHECK_PERMISSION(KRAKEN_PERM_WIFI);
    
    if (!g_wifi.initialized || !g_wifi.enabled || !ssid) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // ... rest of implementation
}
```

### 3. Service Context Tracking

The kernel automatically sets the service context when a service is started/stopped:
- When `kraken_service_start("my_app")` is called, it sets the current service context
- Any API calls made during service initialization inherit this context
- The permission check looks up the calling service and verifies its permissions

## Available Permissions

```c
typedef enum {
    KRAKEN_PERM_NONE = 0,
    KRAKEN_PERM_WIFI = (1 << 0),        // WiFi operations
    KRAKEN_PERM_BT = (1 << 1),          // Bluetooth operations
    KRAKEN_PERM_DISPLAY = (1 << 2),     // Display access
    KRAKEN_PERM_AUDIO = (1 << 3),       // Audio operations
    KRAKEN_PERM_STORAGE = (1 << 4),     // File system access
    KRAKEN_PERM_NETWORK = (1 << 5),     // Network operations
    KRAKEN_PERM_SYSTEM = (1 << 6),      // System-level operations
    KRAKEN_PERM_ALL = 0xFFFFFFFF,       // All permissions
} kraken_permission_t;
```

## Example Usage

### Example 1: App with Limited Permissions

```c
// my_app.c
#include "kraken/kernel.h"
#include "kraken/wifi_service.h"
#include "kraken/bt_service.h"

static esp_err_t my_app_init(void)
{
    // This will succeed because my_app has KRAKEN_PERM_WIFI
    esp_err_t ret = wifi_service_scan();
    if (ret != ESP_OK) {
        ESP_LOGE("my_app", "WiFi scan failed: %d", ret);
        return ret;
    }
    
    // This will FAIL because my_app doesn't have KRAKEN_PERM_BT
    ret = bt_service_scan(10);
    if (ret != ESP_OK) {
        ESP_LOGE("my_app", "BT scan denied (expected): %d", ret);
    }
    
    return ESP_OK;
}

static esp_err_t my_app_deinit(void)
{
    return ESP_OK;
}

void register_my_app(void)
{
    // Register with only WiFi permission
    kraken_service_register("my_app",
                           KRAKEN_PERM_WIFI,  // No Bluetooth permission!
                           my_app_init,
                           my_app_deinit);
}
```

### Example 2: System Service with Full Permissions

```c
// system_manager.c
void register_system_manager(void)
{
    // System manager needs all permissions
    kraken_service_register("system_mgr",
                           KRAKEN_PERM_ALL,
                           system_mgr_init,
                           system_mgr_deinit);
}

static esp_err_t system_mgr_init(void)
{
    // Can call any protected API
    wifi_service_scan();      // OK
    bt_service_scan(10);      // OK
    display_service_clear();  // OK
    
    return ESP_OK;
}
```

### Example 3: Protecting Your Own Service APIs

```c
// my_service.c
#include "kraken/kernel.h"

esp_err_t my_service_critical_operation(void)
{
    // Require system permission for this critical operation
    KRAKEN_CHECK_PERMISSION(KRAKEN_PERM_SYSTEM);
    
    // Only services with KRAKEN_PERM_SYSTEM can reach here
    ESP_LOGI("my_service", "Performing critical operation");
    
    return ESP_OK;
}
```

## Error Handling

When a permission check fails, you'll see logs like:

```
E (12345) kernel_svc: Service 'my_app' denied: missing permission 0x2
E (12346) PERMISSION: Permission denied: required 0x2
```

The API will return `ESP_ERR_INVALID_STATE` to indicate permission was denied.

## Best Practices

1. **Principle of Least Privilege**: Only grant permissions that are actually needed
2. **Protect Sensitive APIs**: Add `KRAKEN_CHECK_PERMISSION` to any API that could be misused
3. **Document Requirements**: Clearly document which permissions are needed for each service
4. **Validate at Boundaries**: Check permissions at service API boundaries, not internal functions
5. **Handle Errors Gracefully**: Always check return codes from permission-protected APIs

## Manual Permission Check

If you need custom permission logic, use the API directly:

```c
esp_err_t kraken_check_caller_permission(kraken_permission_t required_perm);

// Example:
esp_err_t my_function(void)
{
    esp_err_t ret = kraken_check_caller_permission(KRAKEN_PERM_WIFI | KRAKEN_PERM_NETWORK);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Caller needs both WiFi and Network permissions");
        return ESP_ERR_NOT_ALLOWED;
    }
    
    // Proceed with operation
    return ESP_OK;
}
```

## Limitations

- Permission checks only work during service init/deinit callbacks
- Tasks created by services don't automatically inherit the service context
- For long-running tasks, you may need to manually set/restore context using internal APIs

## Security Guarantees

✅ **Permissions cannot be changed after registration**
- Service structure is private (opaque pointer)
- No public API to modify permissions

✅ **Tampering is detected**
- Cryptographic checksum verification
- Security violations are logged

✅ **Type-safe**
- Permission enum prevents invalid values
- Compile-time checking via `KRAKEN_CHECK_PERMISSION` macro

❌ **Not protected against**
- Physical memory attacks (requires hardware security)
- Malicious code with direct memory access
- Kernel-level exploits

For production systems, combine with:
- Code signing
- Secure boot
- Memory protection units (MPU)
- TrustZone or similar secure enclaves
