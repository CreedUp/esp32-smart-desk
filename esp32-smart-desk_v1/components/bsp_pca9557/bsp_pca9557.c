#include "bsp_pca9557.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "pca9557";

static i2c_master_dev_handle_t pca9557_dev = NULL;

/* 保护 PCA9557 的“读取—修改—写回”操作 */
static SemaphoreHandle_t pca9557_mutex = NULL;

/**
 * @brief 写入一个 8 位寄存器
 */
static esp_err_t pca9557_write_reg(
    uint8_t reg,
    uint8_t value
)
{
    if (pca9557_dev == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    const uint8_t write_buffer[2] = {
        reg,
        value
    };

    return i2c_master_transmit(
        pca9557_dev,
        write_buffer,
        sizeof(write_buffer),
        PCA9557_TIMEOUT_MS
    );
}

/**
 * @brief 读取一个 8 位寄存器
 */
static esp_err_t pca9557_read_reg(
    uint8_t reg,
    uint8_t *value
)
{
    if (value == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (pca9557_dev == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    return i2c_master_transmit_receive(
        pca9557_dev,
        &reg,
        sizeof(reg),
        value,
        sizeof(*value),
        PCA9557_TIMEOUT_MS
    );
}

/**
 * @brief 采用读-改-写方式更新寄存器中的指定位
 *
 * @param reg   目标寄存器地址
 * @param mask  哪些位允许被修改
 * @param value 这些位期望设置成什么值
 *
 * @return ESP_OK：修改并验证成功；其他值：失败。
 */
static esp_err_t pca9557_update_reg_bits_unlocked(
    uint8_t reg,
    uint8_t mask,
    uint8_t value
)
{
    uint8_t old_value = 0;

    esp_err_t ret = pca9557_read_reg(
        reg,
        &old_value
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Read register 0x%02X failed: %s",
            (unsigned int)reg,
            esp_err_to_name(ret)
        );

        return ret;
    }

    /*
     * 第一步：
     * old_value & ~mask
     *
     * 将允许修改的位清零，其他位保持不变。
     *
     * 第二步：
     * value & mask
     *
     * 只保留 value 中允许修改的位。
     *
     * 第三步：
     * 使用按位或，将两部分合并。
     */
    uint8_t new_value = (uint8_t)(
        (old_value & (uint8_t)(~mask)) |
        (value & mask)
    );

    /*
     * 如果目标值与原值相同，不需要重复写入。
     * 仍会执行后面的读取验证。
     */
    if (new_value != old_value)
    {
        ret = pca9557_write_reg(
            reg,
            new_value
        );

        if (ret != ESP_OK)
        {
            ESP_LOGE(
                TAG,
                "Write register 0x%02X failed: %s",
                (unsigned int)reg,
                esp_err_to_name(ret)
            );

            return ret;
        }
    }

    uint8_t readback = 0;

    ret = pca9557_read_reg(
        reg,
        &readback
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Readback register 0x%02X failed: %s",
            (unsigned int)reg,
            esp_err_to_name(ret)
        );

        return ret;
    }

    if (readback != new_value)
    {
        ESP_LOGE(
            TAG,
            "Register 0x%02X verify failed: "
            "expected=0x%02X, actual=0x%02X",
            (unsigned int)reg,
            (unsigned int)new_value,
            (unsigned int)readback
        );

        return ESP_ERR_INVALID_RESPONSE;
    }

    ESP_LOGI(
        TAG,
        "Register 0x%02X: old=0x%02X, new=0x%02X",
        (unsigned int)reg,
        (unsigned int)old_value,
        (unsigned int)new_value
    );

    return ESP_OK;
}

/**
 * @brief 线程安全地更新 PCA9557 寄存器中的指定位
 *
 * 使用同一把互斥锁保护完整的：
 * 读取 → 修改 → 写回 → 验证
 */
static esp_err_t pca9557_update_reg_bits(
    uint8_t reg,
    uint8_t mask,
    uint8_t value
)
{
    if (pca9557_mutex == NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    /*
     * 等待获取互斥锁。
     *
     * 最多等待 100 ms。
     * pdMS_TO_TICKS() 将毫秒转换为 FreeRTOS tick。
     */
    BaseType_t taken = xSemaphoreTake(
        pca9557_mutex,
        pdMS_TO_TICKS(100)
    );

    if (taken != pdTRUE)
    {
        ESP_LOGE(TAG, "Take PCA9557 mutex timeout");
        return ESP_ERR_TIMEOUT;
    }

    /*
     * 已经获得互斥锁。
     * 其他任务暂时不能执行同一个寄存器更新流程。
     */
    esp_err_t ret = pca9557_update_reg_bits_unlocked(
        reg,
        mask,
        value
    );

    /*
     * 无论上面的寄存器操作成功还是失败，
     * 都必须释放互斥锁。
     */
    xSemaphoreGive(pca9557_mutex);

    return ret;
}

/**
 * @brief 设置本开发板需要的安全输出状态
 */
static esp_err_t pca9557_configure_safe_state(void)
{
    esp_err_t ret;

    /*
     * 1. 先准备输出锁存值。
     *
     * 只修改 IO0～IO2，保留 IO3～IO7 的原输出锁存值。
     */
    ret = pca9557_update_reg_bits(
        PCA9557_REG_OUTPUT,
        (uint8_t)PCA9557_CONTROL_MASK,
        (uint8_t)PCA9557_SAFE_OUTPUT
    );

    if (ret != ESP_OK)
    {
        return ret;
    }

    /*
     * 2. 关闭 IO0～IO2 的输入极性反相。
     *
     * 这三位即将配置成输出，极性设置不影响输出电平；
     * 这里将其设置为明确状态。
     */
    ret = pca9557_update_reg_bits(
        PCA9557_REG_POLARITY,
        (uint8_t)PCA9557_CONTROL_MASK,
        0x00
    );

    if (ret != ESP_OK)
    {
        return ret;
    }

    /*
     * 3. 最后设置方向。
     *
     * 配置寄存器：
     * 1 = 输入
     * 0 = 输出
     *
     * 将 IO0～IO2 清零，即配置为输出。
     * IO3～IO7 的原方向保持不变。
     */
    ret = pca9557_update_reg_bits(
        PCA9557_REG_CONFIG,
        (uint8_t)PCA9557_CONTROL_MASK,
        0x00
    );

    if (ret != ESP_OK)
    {
        return ret;
    }

    return ESP_OK;
}

esp_err_t bsp_pca9557_init(
    i2c_master_bus_handle_t bus_handle
)
{
    if (bus_handle == NULL)
    {
        return ESP_ERR_INVALID_ARG;
    }

    if (pca9557_dev != NULL)
    {
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t ret = i2c_master_probe(
        bus_handle,
        PCA9557_I2C_ADDRESS,
        PCA9557_TIMEOUT_MS
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Probe failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    const i2c_device_config_t device_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = PCA9557_I2C_ADDRESS,
        .scl_speed_hz = PCA9557_I2C_SPEED_HZ,
        .scl_wait_us = 0,
        .flags = {
            .disable_ack_check = false,
        },
    };

    ret = i2c_master_bus_add_device(
        bus_handle,
        &device_config,
        &pca9557_dev
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Add device failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    pca9557_mutex = xSemaphoreCreateMutex();

    if (pca9557_mutex == NULL)
    {
        ESP_LOGE(TAG, "Failed to create PCA9557 mutex");
        return ESP_ERR_NO_MEM;
    }

    ret = pca9557_configure_safe_state();

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Safe-state configuration failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ESP_LOGI(
        TAG,
        "PCA9557 initialized at address 0x%02X",
        PCA9557_I2C_ADDRESS
    );

    return ESP_OK;
}

esp_err_t bsp_pca9557_dump_registers(void)
{
    uint8_t output = 0;
    uint8_t polarity = 0;
    uint8_t config = 0;

    esp_err_t ret = pca9557_read_reg(
        PCA9557_REG_OUTPUT,
        &output
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Read OUTPUT failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ret = pca9557_read_reg(
        PCA9557_REG_POLARITY,
        &polarity
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Read POLARITY failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ret = pca9557_read_reg(
        PCA9557_REG_CONFIG,
        &config
    );

    if (ret != ESP_OK)
    {
        ESP_LOGE(
            TAG,
            "Read CONFIG failed: %s",
            esp_err_to_name(ret)
        );

        return ret;
    }

    ESP_LOGI(
        TAG,
        "OUTPUT=0x%02X, POLARITY=0x%02X, CONFIG=0x%02X",
        (unsigned int)output,
        (unsigned int)polarity,
        (unsigned int)config
    );

    return ESP_OK;
}

/* high=true：LCD_CS 为高，取消选中屏幕 */
esp_err_t bsp_pca9557_lcd_cs_set(bool high)
{
    return pca9557_update_reg_bits(
        PCA9557_REG_OUTPUT,
        (uint8_t)PCA9557_LCD_CS_MASK,
        high ? (uint8_t)PCA9557_LCD_CS_MASK : 0
    );
}

/* sleep=true：DVP_PWDN 为高，摄像头休眠 */
esp_err_t bsp_pca9557_camera_sleep(bool sleep)
{
    return pca9557_update_reg_bits(
        PCA9557_REG_OUTPUT,
        (uint8_t)PCA9557_DVP_PWDN_MASK,
        sleep ? (uint8_t)PCA9557_DVP_PWDN_MASK : 0
    );
}

/* enable=true：PA_EN 为高，开启扬声器功放 */
esp_err_t bsp_pca9557_speaker_enable(bool enable)
{
    return pca9557_update_reg_bits(
        PCA9557_REG_OUTPUT,
        (uint8_t)PCA9557_PA_EN_MASK,
        enable ? (uint8_t)PCA9557_PA_EN_MASK : 0
    );
}

