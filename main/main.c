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
#include "st7789.h"
#include "st7789_faster.h"

static const char *MAIN_LOG = "MAIN_LOG";

//_Noreturn void taskInfoNES(void *pvParam)
//{
//
//
//    while (1)
//    {
//        while (1)
//        {
//            vTaskDelay(1);
//
//
//        }
//    }
//}

//不知道为什么，这么写会崩溃。。。但是用malloc却没事。
//spi_transaction_t p_spi_transaction_pool[240];

spi_transaction_t *p_spi_transaction_pool;

void initSpiTransactionPool() {
    void *pVoid = malloc(sizeof(spi_transaction_t) * 240);
    memset(pVoid, 0, sizeof(spi_transaction_t) * 240);
    p_spi_transaction_pool = pVoid;
    for (int i = 0; i < 240; i += 1) {
        p_spi_transaction_pool[i].length = CONFIG_WIDTH * 2 * 8;
        p_spi_transaction_pool[i].tx_buffer = &WorkFrame[i * (NES_DISP_WIDTH) + 8];
    }
}

_Noreturn void taskLCD(void *param) {
//    void *pVoid = malloc(80000);
//    ((uint8_t *)pVoid)[80000-1]=22;
    InfoNES_Load(NULL);
    InfoNES_Init();
    FrameSkip++;
    TFT_t dev;

    spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO,
                    CONFIG_BL_GPIO);
    lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);
    initSpiTransactionPool();
    int64_t time_fps_last = esp_timer_get_time();
    while (1) {
        static uint8_t fps_count = 0;
        fps_count++;
        if (fps_count == 0) {
            int64_t time_fps_current = esp_timer_get_time();
            ESP_LOGI(MAIN_LOG, "fps:%lf\n", 256.0 / ((time_fps_current - time_fps_last) / 1000000.0));
            time_fps_last = time_fps_current;
        }
        lcdPrepareMultiPixels(&dev);
        for (int i = 0; i < NES_DISP_HEIGHT; i += 1) {
            spi_device_queue_trans(dev._SPIHandle, &p_spi_transaction_pool[i], portMAX_DELAY);
        }
//        int64_t t_nes_start = esp_timer_get_time();
        InfoNES_Cycle();
//        int64_t t_nes_end = esp_timer_get_time();
        spi_transaction_t *r_trans;
        for (int i = 0; i < NES_DISP_HEIGHT; i += 1) {
            spi_device_get_trans_result(dev._SPIHandle, &r_trans, portMAX_DELAY);
        }
//        ESP_LOGI(MAIN_LOG, "nes:%lld ms\n", (t_nes_end - t_nes_start) / 1000);

        if (fps_count == 0) {

        }
    }
}

_Noreturn void taskSystemMonitor(void *param) {
    uint8_t CPU_RunInfo[2048]; //保存任务运行时间信息

    while (1) {
        //######### 内存 占用信息 #########
        memset(CPU_RunInfo, 0, 2048); //信息缓冲区清零

        vTaskList((char *) &CPU_RunInfo); //获取任务运行时间信息

        ESP_LOGI(MAIN_LOG, "\n---------------------------------------------");
        ESP_LOGI(MAIN_LOG, "\ntask_name\ttask_status\tpriority\tstack task_id\n%s", CPU_RunInfo);
        ESP_LOGI(MAIN_LOG, "total memort free :  %d\n", esp_get_free_heap_size());

        //######### cpu 占用信息 #########
        memset(CPU_RunInfo, 0, 2048);

        vTaskGetRunTimeStats((char *) &CPU_RunInfo); //

        ESP_LOGI(MAIN_LOG, "\ntask_name\trun_cnt\tusage_rate\n%s", CPU_RunInfo);

        vTaskDelay(10000 / portTICK_PERIOD_MS); /* 延时500个tick */
    }
}

_Noreturn void app_main(void) {

    xTaskCreate(taskSystemMonitor, "taskSystemMonitor", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(taskLCD, "taskLCD", 4 * 1024, NULL, 5, NULL);

    while (1) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}