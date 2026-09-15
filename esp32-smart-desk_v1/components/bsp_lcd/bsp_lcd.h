#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

/* LCD 逻辑分辨率：横屏模式 */
#define BSP_LCD_H_RES                 320
#define BSP_LCD_V_RES                 240

/* RGB565常用颜色 */
#define BSP_LCD_COLOR_BLACK           0x0000U
#define BSP_LCD_COLOR_WHITE           0xFFFFU
#define BSP_LCD_COLOR_RED             0xF800U
#define BSP_LCD_COLOR_GREEN           0x07E0U
#define BSP_LCD_COLOR_BLUE            0x001FU

/**
 * @brief 初始化 LCD
 *
 * 初始化过程包括：
 * 1. 配置背光 GPIO
 * 2. 创建 SPI Panel IO
 * 3. 创建 ST7789 Panel
 * 4. 复位并初始化 LCD
 * 5. 设置屏幕显示方向
 * 6. 填充白色
 * 7. 打开显示和背光
 *
 * @return
 *      - ESP_OK：初始化成功
 *      - 其他值：初始化失败
 */
esp_err_t bsp_lcd_init(void);

/**
 * @brief 将整个屏幕填充为指定的 RGB565 颜色
 *
 * @param color RGB565 格式颜色
 *
 * @return
 *      - ESP_OK：填充成功
 *      - ESP_ERR_INVALID_STATE：LCD 尚未初始化
 *      - 其他值：发送显示数据失败
 */
esp_err_t bsp_lcd_fill_color(uint16_t color);

/**
 * @brief 设置 LCD 背光状态
 *
 * @param on
 *      - true：打开背光
 *      - false：关闭背光
 *
 * @return
 *      - ESP_OK：设置成功
 *      - 其他值：GPIO 设置失败
 */
esp_err_t bsp_lcd_backlight_set(bool on);

/**
 * @brief 显示RGB565五色条
 *
 * 从左到右显示：
 * 红、绿、蓝、白、黑。
 *
 * 每个色条宽64像素，高240像素。
 */
esp_err_t bsp_lcd_draw_color_bars(void);

#endif