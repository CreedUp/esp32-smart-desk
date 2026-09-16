#include "bsp_spi.h"

#include <stdint.h>

#include "driver/gpio.h"
#include "driver/spi_master.h"

#include "esp_log.h"

/*
 * LCD SPI 硬件引脚。
 */
#define BSP_SPI_MOSI_GPIO             GPIO_NUM_40
#define BSP_SPI_SCLK_GPIO             GPIO_NUM_41

/*
 * LCD 横屏逻辑尺寸。
 */
#define BSP_LCD_H_RES                 320U
#define BSP_LCD_V_RES                 240U

/*
 * RGB565 每个像素占用两个字节。
 */
#define BSP_LCD_BYTES_PER_PIXEL       sizeof(uint16_t)

/*
 * 一整帧 RGB565 图像：
 *
 * 320 × 240 × 2 = 153600 Byte
 */
#define BSP_SPI_MAX_TRANSFER_SIZE     \
    (BSP_LCD_H_RES *                  \
     BSP_LCD_V_RES *                  \
     BSP_LCD_BYTES_PER_PIXEL)

static const char *TAG = "bsp_spi";

esp_err_t bsp_spi_bus_init(void)
{
    const spi_bus_config_t bus_config = {
        /*
         * 主机向 LCD 发送命令和像素。
         */
        .mosi_io_num = BSP_SPI_MOSI_GPIO,

        /*
         * LCD 没有连接 SPI 读取线路。
         */
        .miso_io_num = -1,

        /*
         * SPI 时钟输出。
         */
        .sclk_io_num = BSP_SPI_SCLK_GPIO,

        /*
         * 不使用 Quad SPI 的 DATA2、DATA3。
         */
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,

        /*
         * 允许一次传输完整的 RGB565 图像。
         */
        .max_transfer_sz = BSP_SPI_MAX_TRANSFER_SIZE,
    };

    esp_err_t ret = spi_bus_initialize(
        BSP_SPI_HOST,
        &bus_config,
        SPI_DMA_CH_AUTO
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "SPI3 bus initialization failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ESP_LOGI(
        TAG,
        "SPI3 initialized: MOSI=%d, SCLK=%d, max_transfer=%u B",
        (int)BSP_SPI_MOSI_GPIO,
        (int)BSP_SPI_SCLK_GPIO,
        (unsigned int)BSP_SPI_MAX_TRANSFER_SIZE
    );

    return ESP_OK;
}