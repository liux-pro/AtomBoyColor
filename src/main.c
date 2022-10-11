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
            vTaskDelay(16 / portTICK_PERIOD_MS);

            uint8_t *buf = lcd_get_buffer();

            for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
            {

                // if (LCD_WIDTH * LCD_HEIGHT  / 3 >i)
                // {
                // ((uint16_t *)(buf))[i] = ST7735_BLUE;
                // }else if (LCD_WIDTH * LCD_HEIGHT * 2 / 3 <i)
                // {
                // ((uint16_t *)(buf))[i] = ST7735_RED;
                // }else{
                // ((uint16_t *)(buf))[i] = ST7735_GREEN;

                // }
                ((uint16_t *)(buf))[i] = i;

                // ((uint16_t *)(buf))[i] = ST7735_RED;
            }
            // /* set window position */
            // lcd_setWindowPosition(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

            // /* activate memory write */
            // lcd_activateMemoryWrite();
            // lcd_framebuffer_send(buf, LCD_WIDTH * LCD_HEIGHT * 2, 2048);
            lcd_fast_refresh();
        }
    }
}

void status_task(void *param)
{
    uint8_t CPU_RunInfo[2048]; //保存任务运行时间信息

    while (1)
    {
        memset(CPU_RunInfo, 0, 2048); //信息缓冲区清零

        vTaskList((char *)&CPU_RunInfo); //获取任务运行时间信息

        ESP_LOGI(MAIN_LOG,"---------------------------------------------\r\n");
        ESP_LOGI(MAIN_LOG,"task_name      task_status priority    stack task_id\r\n");
        ESP_LOGI(MAIN_LOG,"\n%s", CPU_RunInfo);
        ESP_LOGI(MAIN_LOG,"---------------------------------------------\r\n");

        memset(CPU_RunInfo, 0, 2048); //信息缓冲区清零

        vTaskGetRunTimeStats((char *)&CPU_RunInfo);

        ESP_LOGI(MAIN_LOG,"task_name       run_cnt         usage_rate\r\n");
        ESP_LOGI(MAIN_LOG,"\n%s", CPU_RunInfo);
        ESP_LOGI(MAIN_LOG,"---------------------------------------------\r\n\n");
        vTaskDelay(3000 / portTICK_PERIOD_MS); /* 延时500个tick */
    }
}

void app_main(void)
{
    TaskHandle_t taskHandle;

    xTaskCreate(task1, "TASK1", 20 * 1024, NULL, 1, &taskHandle);
    xTaskCreate(status_task, "status_task", 4 * 1024, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}