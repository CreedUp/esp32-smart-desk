#include "bsp_i2c.h"

#include <stdbool.h>
#include <stddef.h>

#include "driver/i2c_master.h"
#include "esp_log.h"

#define BSP_I2C_PORT      I2C_NUM_0
#define BSP_I2C_SDA_GPIO  GPIO_NUM_1
#define BSP_I2C_SCL_GPIO  GPIO_NUM_2

static const char *TAG = "bsp_i2c";

static i2c_master_bus_handle_t i2c_bus_handle = NULL;

esp_err_t bsp_i2c_init(void)
{
    if(i2c_bus_handle != NULL)
    {
        ESP_LOGW(TAG, "I2C bus already initialized");
        return ESP_ERR_INVALID_STATE;
    }

    i2c_master_bus_config_t i2c_bus_config = {
        .i2c_port = BSP_I2C_PORT,
        .sda_io_num = BSP_I2C_SDA_GPIO,
        .scl_io_num = BSP_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = false
        }
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_bus_config, &i2c_bus_handle));

    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    return i2c_bus_handle;
}