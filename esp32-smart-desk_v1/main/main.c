#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

static const char *TAG = "smart_desk";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Smart Desk started");

    while (1) {
        ESP_LOGI(TAG, "System is running");
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
