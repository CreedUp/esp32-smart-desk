idf_component_register(
    SRCS
        "bsp_spi.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_common
        esp_driver_spi

    PRIV_REQUIRES
        esp_driver_gpio
        log
)