idf_component_register(
    SRCS "kraken.c"
    INCLUDE_DIRS "."
    REQUIRES kernel bsp wifi bluetooth audio display system nvs_flash bt
)
