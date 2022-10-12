#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "st7735s.h"
#include "st7735_port.h"

static const char *MAIN_LOG = "MAIN_LOG";
extern uint16_t arr[];

void task1(void *pvParam)
{
    lcd_fast_init();
    while (1)
    {
        while (1)
        {
            // lcd_wait_busy();
            vTaskDelay(16 / portTICK_PERIOD_MS);

            uint8_t *buf = lcd_get_buffer();

            for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
            {
                // ((uint16_t *)(buf))[i] = arr[i];
                buf[i * 2] = ((uint8_t *)arr)[i * 2 + 1];
                buf[i * 2 + 1] = ((uint8_t *)arr)[i * 2];
            }
            lcd_fast_refresh();
        }
    }
}

void status_task(void *param)
{
    uint8_t CPU_RunInfo[2048]; //保存任务运行时间信息

    while (1)
    {
        //######### 内存 占用信息 #########
        memset(CPU_RunInfo, 0, 2048); //信息缓冲区清零

        vTaskList((char *)&CPU_RunInfo); //获取任务运行时间信息

        ESP_LOGI(MAIN_LOG, "\n---------------------------------------------");
        ESP_LOGI(MAIN_LOG, "\ntask_name\ttask_status\tpriority\tstack task_id\n%s", CPU_RunInfo);
        ESP_LOGI(MAIN_LOG, "total memort free :  %d\n", esp_get_free_heap_size());

        //######### cpu 占用信息 #########
        memset(CPU_RunInfo, 0, 2048);

        vTaskGetRunTimeStats((char *)&CPU_RunInfo); //

        ESP_LOGI(MAIN_LOG, "\ntask_name\trun_cnt\tusage_rate\n%s", CPU_RunInfo);

        vTaskDelay(10000 / portTICK_PERIOD_MS); /* 延时500个tick */
    }
}

void app_main(void)
{
    TaskHandle_t taskHandle;

    xTaskCreate(task1, "TASK1", 5 * 1024, NULL, 1, &taskHandle);
    xTaskCreate(status_task, "status_task", 4 * 1024, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}