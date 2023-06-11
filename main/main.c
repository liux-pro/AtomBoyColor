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
#include <driver/i2s_common.h>
#include <driver/i2s_std.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "InfoNES_C.h"
#include "InfoNES.h"
#include "InfoNES_Types.h"

#include "roms/roms.h"
#include "lcd.h"
#include "taskMonitor.h"
#include "timeProbe.h"

#define MAIN_LOG_TAG "GameLite:main"

extern EXT_RAM_BSS_ATTR uint8_t rom[];
extern const uint8_t logo[];
EXT_RAM_BSS_ATTR uint16_t frameBuffer[240 * 240];

extern DWORD dwPad1;
extern DWORD dwPad2;

#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF  ESP_IF_WIFI_STA
#define CONFIG_ESPNOW_CHANNEL  10

timeProbe_t fps;
TaskHandle_t handle_taskInfoNES;
TaskHandle_t handle_taskFlush;
TaskHandle_t handle_taskSound;


#define PAD_KEY_A       0
#define PAD_KEY_B       1
#define PAD_KEY_SELECT  2
#define PAD_KEY_START   3
#define PAD_KEY_UP      4
#define PAD_KEY_DOWN    5
#define PAD_KEY_LEFT    6
#define PAD_KEY_RIGHT   7


_Noreturn void taskInfoNES(void *param) {
    vTaskDelay(pdMS_TO_TICKS(100));

    memcpy(rom, roms_snow_start, roms_snow_end - roms_snow_start);

    InfoNES_Load_C();
    InfoNES_Init_C();

//    FrameSkip++;
    int64_t lastFrameTime = 0;

    while (1) {
        int64_t current = esp_timer_get_time();
        int64_t shouldFlushTime = lastFrameTime + (1000 * 1000 / 60);
        if (shouldFlushTime > current) {
            vTaskDelay(pdMS_TO_TICKS((shouldFlushTime - current) >> 10));
            lastFrameTime = shouldFlushTime;
        } else {
            lastFrameTime=current;
        }
        InfoNES_Cycle_C();

        xTaskNotifyGive(handle_taskFlush);
        xTaskNotifyGive(handle_taskSound);
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
        WORD *localWorkFrame = InfoNES_GetScreenBuffer_C();
        for (int y = 0; y < LCD_HEIGHT; ++y) {
            for (int x = 0; x < LCD_WIDTH; ++x) {
                frameBuffer[x + y * (LCD_WIDTH)] = localWorkFrame[x + y * (NES_DISP_WIDTH) + 8];
            }
        }

        esp_lcd_panel_draw_bitmap(lcd_panel_handle, 0, 0, LCD_WIDTH, LCD_HEIGHT, frameBuffer);

    }

}


#define I2S_BCLK        GPIO_NUM_1      // I2S bit clock io number
#define I2S_LRC         GPIO_NUM_3      // I2S word select io number
#define I2S_DOUT        GPIO_NUM_2     // I2S data out io number
#define I2S_DIN         GPIO_NUM_NC    // I2S data in io number
#define EXAMPLE_BUFF_SIZE               735  // InfoNES 每次都是给出735个采样点
#define SAMPLE_RATE                    44100      // InfoNES 定了
static i2s_chan_handle_t tx_chan;        // I2S tx channel handler


static void i2s_init(void) {
    i2s_chan_config_t tx_chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&tx_chan_cfg, &tx_chan, NULL));


    i2s_std_config_t tx_std_cfg = {
            .clk_cfg  = I2S_STD_CLK_DEFAULT_CONFIG(SAMPLE_RATE),
            .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,
                                                        I2S_SLOT_MODE_MONO),

            .gpio_cfg = {
                    .mclk = I2S_GPIO_UNUSED,    // some codecs may require mclk signal, this example doesn't need it
                    .bclk = I2S_BCLK,
                    .ws   = I2S_LRC,
                    .dout = I2S_DOUT,
                    .din  = I2S_DIN,
                    .invert_flags = {
                            .mclk_inv = false,
                            .bclk_inv = false,
                            .ws_inv   = false,
                    },
            },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &tx_std_cfg));
}


void taskSound(void *parm) {
    i2s_init();
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    size_t w_bytes = 0;
    while (1) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        /* Write i2s data */
        if (i2s_channel_write(tx_chan, InfoNES_GetSoundBuffer_C(), EXAMPLE_BUFF_SIZE * 2, &w_bytes, 1000) == ESP_OK) {
//            printf("Write Task: i2s write %d bytes\n", w_bytes);
        } else {
            printf("Write Task: i2s write failed\n");
        }
    }


    ESP_ERROR_CHECK(i2s_channel_disable(tx_chan));
    vTaskDelete(NULL);


}

static void example_espnow_recv_cb(const esp_now_recv_info_t * esp_now_info, const uint8_t *data, int data_len) {
    dwPad1 = *((uint8_t *) (data) + 0);
    if (dwPad1 == 0) {
        gpio_set_level(GPIO_NUM_10, 0);
    } else {
        gpio_set_level(GPIO_NUM_10, 1);
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

    xTaskCreatePinnedToCore(taskFlush, "taskFlush", 4 * 1024, NULL, 5, &handle_taskFlush, 0);
    xTaskCreatePinnedToCore(taskSound, "taskSound", 4 * 1024, NULL, 5, &handle_taskSound, 0);
    xTaskCreatePinnedToCore(taskInfoNES, "taskInfoNES", 4 * 1024, NULL, 5, &handle_taskInfoNES, 1);
}