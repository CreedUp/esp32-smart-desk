#ifndef BSP_I2C_H
#define BSP_I2C_H

#include "esp_err.h"
#include "driver/i2c_types.h"

#define PCA9557_I2C_ADDRESS       0x19
#define I2C_PROBE_TIMEOUT_MS      100

/**
 * @brief 初始化板载共享 I2C 总线
 *
 * 仅在启动阶段调用，不支持并发初始化。
 *
 * @return
 *     ESP_OK：初始化成功。
 *     ESP_ERR_INVALID_STATE：已经初始化。
 *     其他错误码：底层驱动返回的错误。
 */
esp_err_t bsp_i2c_init(void);

/**
 * @brief 获取共享 I2C 总线句柄
 *
 * @return 初始化成功后返回总线句柄，尚未初始化时返回 NULL。
 *
 * 调用者只借用该句柄，不应删除共享总线。
 */
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

#endif