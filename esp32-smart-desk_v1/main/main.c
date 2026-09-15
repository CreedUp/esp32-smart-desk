#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

#include "system_monitor.h"

static const char *TAG = "smart_desk";

void app_main(void)
{
    ESP_LOGI(TAG, "ESP32 Smart Desk started");

    ESP_ERROR_CHECK(system_monitor_start());

}