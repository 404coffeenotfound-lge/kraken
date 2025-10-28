idf_component_register(
    SRCS "system_service.c"
    INCLUDE_DIRS "include"
    REQUIRES lwip esp_netif bsp kernel
)
