#ifndef BSP_PCA9557_H
#define BSP_PCA9557_H

#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_types.h"

/*
 * PCA9557 的 I2C 通信参数。
 */
#define PCA9557_I2C_ADDRESS       0x19
#define PCA9557_I2C_SPEED_HZ      100000
#define PCA9557_TIMEOUT_MS        100

/*
 * PCA9557 寄存器地址。
 */
#define PCA9557_REG_INPUT         0x00
#define PCA9557_REG_OUTPUT        0x01
#define PCA9557_REG_POLARITY      0x02
#define PCA9557_REG_CONFIG        0x03

/*
 * 本开发板使用的 PCA9557 引脚。
 */
#define PCA9557_LCD_CS_MASK       (1U << 0)
#define PCA9557_PA_EN_MASK        (1U << 1)
#define PCA9557_DVP_PWDN_MASK     (1U << 2)

/*
 * IO0、IO1、IO2 的组合掩码：
 *
 * 0000 0001
 * 0000 0010
 * 0000 0100
 * -----------
 * 0000 0111 = 0x07
 */
#define PCA9557_CONTROL_MASK      \
    (PCA9557_LCD_CS_MASK |        \
     PCA9557_PA_EN_MASK |         \
     PCA9557_DVP_PWDN_MASK)

/*
 * 安全输出状态：
 *
 * IO0 LCD_CS   = 1：不选中屏幕
 * IO1 PA_EN    = 0：关闭功放
 * IO2 DVP_PWDN = 1：摄像头休眠
 *
 * 0000 0101 = 0x05
 */
#define PCA9557_SAFE_OUTPUT       \
    (PCA9557_LCD_CS_MASK |        \
     PCA9557_DVP_PWDN_MASK)

/**
 * @brief 初始化 PCA9557
 *
 * 初始化过程：
 * 1. 探测设备地址。
 * 2. 创建 I2C 设备对象。
 * 3. 设置安全输出状态。
 * 4. 设置输入极性。
 * 5. 配置 IO0、IO1、IO2 为输出。
 * 6. 读回寄存器验证。
 *
 * 安全状态：
 * - LCD_CS = 1：不选中屏幕。
 * - PA_EN = 0：关闭功放。
 * - DVP_PWDN = 1：摄像头休眠。
 *
 * @param bus_handle 已经初始化的共享 I2C 总线句柄
 *
 * @return
 *     ESP_OK：初始化并验证成功。
 *     ESP_ERR_INVALID_ARG：总线句柄为空。
 *     ESP_ERR_INVALID_STATE：已经初始化。
 *     ESP_ERR_INVALID_RESPONSE：读回值与期望值不一致。
 *     其他错误码：I2C 驱动返回的错误。
 */
esp_err_t bsp_pca9557_init(
    i2c_master_bus_handle_t bus_handle
);

/**
 * @brief 读取并打印 PCA9557 配置寄存器
 *
 * @return ESP_OK：读取成功；其他值：读取失败。
 */
esp_err_t bsp_pca9557_dump_registers(void);

/**
 * @brief 设置 LCD 片选信号
 *
 * LCD_CS 连接到 PCA9557 的 IO0，并且低电平有效。
 *
 * @param high
 *     true：IO0 输出高电平，取消选中 LCD。
 *     false：IO0 输出低电平，选中 LCD。
 *
 * @return
 *     ESP_OK：设置成功。
 *     ESP_ERR_INVALID_STATE：PCA9557 尚未初始化。
 *     其他错误码：I2C 读写失败。
 */
esp_err_t bsp_pca9557_lcd_cs_set(bool high);

/**
 * @brief 设置摄像头休眠状态
 *
 * DVP_PWDN 连接到 PCA9557 的 IO2，并且高电平进入休眠。
 *
 * @param sleep
 *     true：IO2 输出高电平，摄像头进入休眠。
 *     false：IO2 输出低电平，摄像头退出休眠。
 *
 * @return
 *     ESP_OK：设置成功。
 *     ESP_ERR_INVALID_STATE：PCA9557 尚未初始化。
 *     其他错误码：I2C 读写失败。
 */
esp_err_t bsp_pca9557_camera_sleep(bool sleep);

/**
 * @brief 设置扬声器功放使能状态
 *
 * PA_EN 连接到 PCA9557 的 IO1，并且高电平使能功放。
 *
 * @param enable
 *     true：IO1 输出高电平，开启功放。
 *     false：IO1 输出低电平，关闭功放。
 *
 * @return
 *     ESP_OK：设置成功。
 *     ESP_ERR_INVALID_STATE：PCA9557 尚未初始化。
 *     其他错误码：I2C 读写失败。
 */
esp_err_t bsp_pca9557_speaker_enable(bool enable);

#endif
