#include <sys/cdefs.h>
#include <sys/select.h>
#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <esp_task_wdt.h>
#include <nvs_flash.h>
#include <esp_now.h>
#include <esp_netif.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "InfoNES_System.h"
#include "InfoNES.h"
#include "roms/roms.h"
#include "lcd.h"
#include "taskMonitor.h"
#include "timeProbe.h"

#define MAIN_LOG_TAG "GameLite:main"

extern EXT_RAM_BSS_ATTR uint8_t rom[];
extern const uint8_t logo[];
uint16_t frameBuffer[240 * 240];

extern DWORD dwPad1;
extern DWORD dwPad2;

#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF  ESP_IF_WIFI_STA
#define CONFIG_ESPNOW_CHANNEL  10

timeProbe_t fps;
TaskHandle_t handle_taskLCD;
TaskHandle_t handle_taskFlush;

WORD *currentWorkFrame;


#define PAD_KEY_A       0
#define PAD_KEY_B       1
#define PAD_KEY_SELECT  2
#define PAD_KEY_START   3
#define PAD_KEY_UP      4
#define PAD_KEY_DOWN    5
#define PAD_KEY_LEFT    6
#define PAD_KEY_RIGHT   7

bool switching_game = true;

_Noreturn void taskLCD(void *param) {
    vTaskDelay(pdMS_TO_TICKS(100));

    do {
        switch (dwPad1) {
            case 1 << PAD_KEY_UP: {
                memcpy(rom, roms_contra_start, roms_contra_end - roms_contra_start);
                switching_game = false;
                break;
            }
            case 1 << PAD_KEY_DOWN: {
                memcpy(rom, roms_mario_start, roms_mario_end - roms_mario_start);
                switching_game = false;
                break;
            }
            case 1 << PAD_KEY_LEFT: {
                memcpy(rom, roms_tank_start, roms_tank_end - roms_tank_start);
                switching_game = false;
                break;
            }
            default: {
                vTaskDelay(pdMS_TO_TICKS(100));
                memcpy(frameBuffer, logo, LCD_WIDTH * LCD_HEIGHT * 2);
                esp_lcd_panel_draw_bitmap(lcd_panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, frameBuffer);
            }
        }

    } while (switching_game);


    InfoNES_Load(NULL);
    InfoNES_Init();
//    FrameSkip++;

    while (1) {
        InfoNES_Cycle();
        currentWorkFrame = DoubleFrame[WorkFrameIdx];

        xTaskNotifyGive(handle_taskFlush);
    }
}

void setInput(gpio_num_t gpio_num) {
    gpio_reset_pin(gpio_num);
    gpio_pullup_en(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_INPUT);
}

void setOutput(gpio_num_t gpio_num) {
    gpio_reset_pin(gpio_num);
    gpio_set_direction(gpio_num, GPIO_MODE_OUTPUT);
}

void taskFlush(void *parm) {
    uint8_t fps_count = 0;
    timeProbe_start(&fps);

    while (1) {
        fps_count++;
        if (fps_count == 0) {
            ESP_LOGI(MAIN_LOG_TAG, "fps: %f", 256.0f * 1000.0f / (timeProbe_stop(&fps) / 1000.0));
            timeProbe_start(&fps);
        }
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        WORD *localWorkFrame = currentWorkFrame;
        for (int y = 0; y < LCD_HEIGHT; ++y) {
            for (int x = 0; x < LCD_WIDTH; ++x) {
                frameBuffer[x + y * (LCD_WIDTH)] = localWorkFrame[x + y * (NES_DISP_WIDTH) + 8];
            }
        }
        if (switching_game) {
            memcpy(frameBuffer, logo, LCD_WIDTH * LCD_HEIGHT * 2);
        }
        esp_lcd_panel_draw_bitmap(lcd_panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, frameBuffer);

    }

}

static void example_espnow_recv_cb(const uint8_t *mac_addr, const uint8_t *data, int data_len) {
    dwPad1 = *((uint8_t *) (data) + 0);
    if (dwPad1==0){
        gpio_set_level(GPIO_NUM_10,0);
    } else{
        gpio_set_level(GPIO_NUM_10,1);
    }
}


static esp_err_t example_espnow_init(void) {
    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(example_espnow_recv_cb));
    return ESP_OK;
}

static void example_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(ESPNOW_WIFI_MODE));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}

void app_main(void) {
    startTaskMonitor(10000);
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    example_wifi_init();
    example_espnow_init();
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

    setOutput(GPIO_NUM_10);

    xTaskCreatePinnedToCore(taskFlush, "taskFlush", 4 * 1024, NULL, 5, &handle_taskFlush, 1);
    xTaskCreatePinnedToCore(taskLCD, "taskLCD", 4 * 1024, NULL, 5, &handle_taskLCD, 0);
}