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
uint16_t frameBuffer[240 * 240];
extern DWORD dwPad1;

#define PAD_KEY_A       0
#define PAD_KEY_B       1
#define PAD_KEY_SELECT  2
#define PAD_KEY_START   3
#define PAD_KEY_UP      4
#define PAD_KEY_DOWN    5
#define PAD_KEY_LEFT    6
#define PAD_KEY_RIGHT   7

void setBit(uint32_t *data, uint8_t location, bool x) {
    *data &= ~(1 << location);  // Clear the bit at the specified location
    *data |= (x << location);   // Set the bit to x at the specified location
}

timeProbe_t fps;

_Noreturn void taskLCD(void *param) {
    InfoNES_Load(NULL);
    InfoNES_Init();
//    FrameSkip++;

    while (1) {
        timeProbe_start(&fps);
        setBit(&dwPad1, PAD_KEY_A, !gpio_get_level(GPIO_NUM_1));
        setBit(&dwPad1, PAD_KEY_B, !gpio_get_level(GPIO_NUM_2));
        setBit(&dwPad1, PAD_KEY_SELECT, !gpio_get_level(GPIO_NUM_3));
        setBit(&dwPad1, PAD_KEY_START, !gpio_get_level(GPIO_NUM_4));
        setBit(&dwPad1, PAD_KEY_UP, !gpio_get_level(GPIO_NUM_5));
        setBit(&dwPad1, PAD_KEY_DOWN, !gpio_get_level(GPIO_NUM_6));
        setBit(&dwPad1, PAD_KEY_LEFT, !gpio_get_level(GPIO_NUM_7));
        setBit(&dwPad1, PAD_KEY_RIGHT, !gpio_get_level(GPIO_NUM_8));
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

void setInput(gpio_num_t gpio_num) {
    gpio_reset_pin(gpio_num);
    gpio_pullup_en(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
}


void app_main(void) {
    startTaskMonitor(10000);

    if (ESP_OK != init_lcd()) {
        ESP_LOGE(MAIN_LOG_TAG, "LCD init fail");
        while (1);
    }
    setInput(GPIO_NUM_1);
    setInput(GPIO_NUM_2);
    setInput(GPIO_NUM_3);
    setInput(GPIO_NUM_4);
    setInput(GPIO_NUM_5);
    setInput(GPIO_NUM_6);
    setInput(GPIO_NUM_7);
    setInput(GPIO_NUM_8);


    xTaskCreate(taskLCD, "taskLCD", 4 * 1024, NULL, 5, NULL);

}