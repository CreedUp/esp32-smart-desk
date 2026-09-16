#ifndef APP_UI_H
#define APP_UI_H

#include "esp_err.h"

/**
 * @brief 初始化LVGL和应用界面
 *
 * @return
 *      - ESP_OK：初始化成功
 *      - 其他值：初始化失败
 */
esp_err_t app_ui_init(void);

#endif