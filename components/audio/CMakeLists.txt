idf_component_register(
    SRCS "audio_service.c"
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_gpio bsp esp_http_client
)
