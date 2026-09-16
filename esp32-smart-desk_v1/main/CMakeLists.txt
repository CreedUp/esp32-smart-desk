idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        system_monitor
        bsp_i2c
        bsp_pca9557
        bsp_spi
        bsp_lcd
        app_ui
        freertos
        esp_common
        log
        esp_psram
)