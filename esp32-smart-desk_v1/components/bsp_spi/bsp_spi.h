#ifndef BSP_SPI_H
#define BSP_SPI_H

#include "esp_err.h"
#include "driver/spi_master.h"

/*
 * 使用 ESP32-S3 的 SPI3 控制器。
 *
 * bsp_lcd 后续创建 Panel IO 时需要知道 LCD 位于哪条 SPI 总线上，
 * 因此 BSP_SPI_HOST 保留在公共头文件中。
 */
#define BSP_SPI_HOST    SPI3_HOST

/**
 * @brief 初始化板载 LCD 使用的 SPI3 总线
 *
 * 初始化内容：
 * 1. MOSI 使用 GPIO40。
 * 2. SCLK 使用 GPIO41。
 * 3. 不使用 MISO。
 * 4. 配置整帧 RGB565 最大传输长度。
 * 5. 自动分配 DMA 通道。
 *
 * 本函数只初始化 SPI 总线，不会初始化 ST7789。
 *
 * @return
 *     ESP_OK：初始化成功。
 *     ESP_ERR_INVALID_ARG：配置参数无效。
 *     ESP_ERR_INVALID_STATE：SPI3 已经初始化。
 *     ESP_ERR_NOT_FOUND：没有可用的 DMA 通道。
 *     ESP_ERR_NO_MEM：内存不足。
 */
esp_err_t bsp_spi_bus_init(void);

#endif