idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        system_monitor
        bsp_i2c
        bsp_pca9557
        freertos
        esp_common
        log
        esp_psram
)