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

void task1(void *pvParam)
{
    lcd_fast_init();
    while (1)
    {
        while (1)
        {
            uint8_t *buf = lcd_get_buffer();

            for (size_t i = 0; i < LCD_WIDTH * LCD_HEIGHT; i++)
            {
                ((uint16_t *)(buf))[i] = ST7735_BLUE;
            }
            // /* set window position */
            // lcd_setWindowPosition(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);

            // /* activate memory write */
            // lcd_activateMemoryWrite();
            // lcd_framebuffer_send(buf, LCD_WIDTH * LCD_HEIGHT * 2, 2048);
            lcd_send_buffer();
            vTaskDelay(50 / portTICK_PERIOD_MS);
        }
    }
}

void status_task(void *param)
{
    char pbuffer[2048] = {0};
    while (1)
    {
        ESP_LOGI(MAIN_LOG, "-------------------- heap:%u --------------------------\r\n", esp_get_free_heap_size());
        vTaskList(pbuffer);
        ESP_LOGI(MAIN_LOG, "%s", pbuffer);
        ESP_LOGI(MAIN_LOG, "----------------------------------------------\r\n");
        vTaskDelay(3000 / portTICK_RATE_MS);
    }
}

void app_main(void)
{
    TaskHandle_t taskHandle;

    xTaskCreate(task1, "TASK1", 80 * 1024, NULL, 1, &taskHandle);
    xTaskCreate(status_task, "status_task", 4 * 1024, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}