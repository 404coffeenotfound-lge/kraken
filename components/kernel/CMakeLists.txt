idf_component_register(
    SRCS "kernel.c" 
         "kernel_service.c"
         "kernel_event.c"
         "kernel_memory.c"
         "kernel_timer.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "."
    REQUIRES esp_timer esp_common freertos
)
