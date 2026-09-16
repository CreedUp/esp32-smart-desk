#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "system_monitor.h"
#include "bsp_i2c.h"
#include "bsp_pca9557.h"
#include "bsp_spi.h"
#include "bsp_lcd.h"
#include "app_ui.h"

#include "driver/i2c_master.h"

static const char *TAG = "smart_desk";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Smart Desk started");

    ESP_ERROR_CHECK(system_monitor_start());
    ESP_ERROR_CHECK(bsp_i2c_init());

    i2c_master_bus_handle_t i2c_bus_handle = bsp_i2c_get_handle();

    ESP_ERROR_CHECK(bsp_pca9557_init(i2c_bus_handle));

    ESP_ERROR_CHECK(
        bsp_spi_bus_init()
    );

    ESP_ERROR_CHECK(
        bsp_lcd_init()
    );
    
    ESP_ERROR_CHECK(
        app_ui_init()
    );

    ESP_ERROR_CHECK(bsp_pca9557_dump_registers());

}