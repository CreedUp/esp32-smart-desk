idf_component_register(
    SRCS
        "bsp_lcd.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_common

    PRIV_REQUIRES
        bsp_spi
        bsp_pca9557
        esp_driver_gpio
        esp_lcd
        log
)