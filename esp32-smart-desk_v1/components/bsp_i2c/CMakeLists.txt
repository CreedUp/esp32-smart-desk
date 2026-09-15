idf_component_register(
    SRCS "bsp_i2c.c"
    INCLUDE_DIRS "."
    REQUIRES
        esp_common
        esp_driver_gpio
        esp_driver_i2c
    PRIV_REQUIRES
        freertos
        log
)