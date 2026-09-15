#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "system_monitor.h"
#include "bsp_i2c.h"

#include "driver/i2c_master.h"

static const char *TAG = "smart_desk";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Smart Desk started");

    ESP_ERROR_CHECK(system_monitor_start());
    ESP_ERROR_CHECK(bsp_i2c_init());

    i2c_master_bus_handle_t i2c_bus_handle = bsp_i2c_get_handle();

    esp_err_t ret = i2c_master_probe(
        i2c_bus_handle,
        PCA9557_I2C_ADDRESS,
        I2C_PROBE_TIMEOUT_MS
    );

    if(ret != ESP_OK)
    {
        ESP_LOGW(TAG, "PCA9557 get failed");
    }
    else
    {
        ESP_LOGI(TAG, "PCA9557 on bus !");
    }

}