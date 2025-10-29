idf_component_register(
    SRCS "bt_service.c" "bt_profiles.c"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_event nvs_flash kernel
)
