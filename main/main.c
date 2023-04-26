#include <sys/cdefs.h>
#include <sys/select.h>
#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "InfoNES_System.h"
#include "InfoNES.h"
#include "lcd.h"
#include "taskMonitor.h"
#include "timeProbe.h"

#define MAIN_LOG_TAG "GameLite:main"
EXT_RAM_BSS_ATTR uint16_t frameBuffer[240 * 240];


timeProbe_t fps;

_Noreturn void taskLCD(void *param) {
    InfoNES_Load(NULL);
    InfoNES_Init();
    FrameSkip++;

    int64_t time_fps_last = esp_timer_get_time();
    while (1) {
        timeProbe_start(&fps);
        InfoNES_Cycle();
        for (int y = 0; y < LCD_HEIGHT; ++y) {
            for (int x = 0; x < LCD_WIDTH; ++x) {
                frameBuffer[x + y * (LCD_WIDTH)] = WorkFrame[x + y * (NES_DISP_WIDTH) + 8];
            }
        }

        esp_lcd_panel_draw_bitmap(lcd_panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, frameBuffer);
        ESP_LOGI(MAIN_LOG_TAG, "fps: %f", 1000 / (timeProbe_stop(&fps) / 1000.0));
    }
}


void app_main(void) {
    startTaskMonitor(10000);

    if (ESP_OK != init_lcd()) {
        ESP_LOGE(MAIN_LOG_TAG, "LCD init fail");
        while (1);
    }

    xTaskCreate(taskLCD, "taskLCD", 4 * 1024, NULL, 5, NULL);

}