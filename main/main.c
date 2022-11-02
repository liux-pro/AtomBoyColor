#include <sys/select.h>
#include <sys/cdefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "sdkconfig.h"
#include "InfoNES_System.h"
#include "InfoNES.h"
#include "st7789.h"

static const char *MAIN_LOG = "MAIN_LOG";
extern unsigned char pic[];

//_Noreturn void task1(void *pvParam)
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

_Noreturn void infoNES_task(void *param){
    InfoNES_Load(NULL);
    InfoNES_Init();
    TFT_t dev;

    spi_master_init(&dev, CONFIG_MOSI_GPIO, CONFIG_SCLK_GPIO, CONFIG_CS_GPIO, CONFIG_DC_GPIO, CONFIG_RESET_GPIO, CONFIG_BL_GPIO);
    lcdInit(&dev, CONFIG_WIDTH, CONFIG_HEIGHT, CONFIG_OFFSETX, CONFIG_OFFSETY);
    while (1){
        int64_t t0 = esp_timer_get_time();
        InfoNES_Cycle();
        int64_t t1 = esp_timer_get_time();
        for (int i = 0; i < 240; ++i) {
            lcdDrawMultiPixels(&dev,0,i,240,&WorkFrame[i*(NES_DISP_WIDTH)]);
        }
        int64_t t2 = esp_timer_get_time();
        ESP_LOGI(MAIN_LOG, "nes:%lld ms\tlcd:%lld ms\n", (t1-t0)/1000,(t2-t1)/1000);
    }
}

_Noreturn void status_task(void *param)
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

//    xTaskCreate(task1, "TASK1", 5 * 1024, NULL, 1, &taskHandle);
    xTaskCreate(status_task, "status_task", 4 * 1024, NULL, 5, NULL);
    xTaskCreate(infoNES_task, "infoNES_task", 4 * 1024, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}