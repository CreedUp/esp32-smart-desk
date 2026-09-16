idf_component_register(
    SRCS
        "app_ui.c"

    INCLUDE_DIRS
        "."

    REQUIRES
        esp_common

    PRIV_REQUIRES
        bsp_lcd
        esp_lcd
        esp_lvgl_port
        log
)