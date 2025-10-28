#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t system_service_init(void);
esp_err_t system_service_deinit(void);

esp_err_t system_service_sync_time(const char *ntp_server, const char *timezone);
esp_err_t system_service_get_time(struct tm *timeinfo);

esp_err_t system_service_start_input_monitor(void);
esp_err_t system_service_stop_input_monitor(void);

#ifdef __cplusplus
}
#endif
