/**
 * Stub implementation for esp_wifi_sta_get_rsnxe
 * 
 * This function is required by wpa_supplicant but not provided by ESP-IDF
 * when WPA3 is disabled. We provide a stub that returns NULL to indicate
 * WPA3/RSN Extension is not supported.
 */

#include <stdint.h>
#include <stddef.h>

uint8_t* esp_wifi_sta_get_rsnxe(uint8_t *bssid)
{
    // Return NULL to indicate WPA3 RSNXE not supported
    // This is the correct behavior when WPA3 is disabled
    return NULL;
}
