idf_component_register(
    SRCS "bsp_pca9557.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_common
        esp_driver_i2c
    PRIV_REQUIRES
        freertos
        log
)