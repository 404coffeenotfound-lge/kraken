idf_component_register(
    SRCS "bt_service.c"
    INCLUDE_DIRS "include"
    REQUIRES bt esp_event nvs_flash kernel
)
