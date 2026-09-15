#ifndef SYSTEM_MONITOR_H
#define SYSTEM_MONITOR_H

#include "esp_err.h"

/**
 * @brief 启动系统监测任务
 *
 * 在系统启动阶段调用，不支持多个任务并发调用。
 *
 * @return
 *     ESP_OK：任务创建成功。
 *     ESP_ERR_INVALID_STATE：任务已经启动。
 *     ESP_ERR_NO_MEM：任务创建失败，内存不足。
 */
esp_err_t system_monitor_start(void);

#endif