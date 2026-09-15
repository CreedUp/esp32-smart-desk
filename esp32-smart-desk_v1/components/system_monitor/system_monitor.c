#include "system_monitor.h"

#include <stddef.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_heap_caps.h"
#include "esp_log.h"

#define SYSTEM_MONITOR_STACK_SIZE_BYTES  4096
#define SYSTEM_MONITOR_PRIORITY          2
#define SYSTEM_MONITOR_PERIOD_MS         5000

static const char *TAG = "sys_monitor";

static TaskHandle_t system_monitor_task_handle = NULL;

static void system_monitor_task(void *arg)
{
    (void)arg;

    while (1)
    {
        // 当前空闲的内部 RAM，要求支持按字节访问。
        size_t internal_free = heap_caps_get_free_size(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        );

        // 内部 RAM 中最大的单个空闲块。
        size_t internal_largest = heap_caps_get_largest_free_block(
            MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT
        );

        // 当前空闲的 PSRAM，要求支持按字节访问。
        size_t psram_free = heap_caps_get_free_size(
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        );

        // 当前任务自创建以来，最少剩余过多少栈空间。
        UBaseType_t stack_min_free =
            uxTaskGetStackHighWaterMark(NULL);

        ESP_LOGI(
            TAG,
            "Internal RAM: free=%zu B, largest=%zu B",
            internal_free,
            internal_largest
        );

        ESP_LOGI(
            TAG,
            "PSRAM: free=%zu B",
            psram_free
        );

        ESP_LOGI(
            TAG,
            "Monitor stack: min_free=%u B",
            (unsigned int)stack_min_free
        );

        vTaskDelay(pdMS_TO_TICKS(SYSTEM_MONITOR_PERIOD_MS));
    }
}

esp_err_t system_monitor_start(void)
{
    if (system_monitor_task_handle != NULL)
    {
        ESP_LOGW(TAG, "System monitor already started");
        return ESP_ERR_INVALID_STATE;
    }

    BaseType_t result = xTaskCreate(
        system_monitor_task,
        "system_monitor_task",
        SYSTEM_MONITOR_STACK_SIZE_BYTES,
        NULL,
        SYSTEM_MONITOR_PRIORITY,
        &system_monitor_task_handle
    );

    if (result != pdPASS)
    {
        ESP_LOGE(TAG, "Failed to create system monitor task");
        return ESP_ERR_NO_MEM;
    }

    return ESP_OK;
}