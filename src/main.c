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

#define LCD_HOST SPI2_HOST

#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 3
#define PIN_NUM_CLK 2
#define PIN_NUM_CS 7

#define PIN_NUM_BL 11
#define PIN_NUM_DC 8
#define PIN_NUM_RST 10

#define LCD_WIDTH 128
#define LCD_HEIGHT 160

#define ST7735_BLACK 0x0000
#define ST7735_BLUE 0x001F
#define ST7735_RED 0xF800
#define ST7735_GREEN 0x07E0

spi_device_handle_t spi;

//这三个方法被st7735的通用驱动（https://github.com/michal037/driver-ST7735S）以extern的方式引用，事实上，驱动只需要这三个函数
/* ######################## HAVE TO BE IMPLEMENTED ########################## */
/* These three functions HAVE TO BE LINKED to the code from the st7735s.c file.
 * They can be implemented in another file than this one.
 */

void lcd_delay(unsigned long int milliseconds)
{
    vTaskDelay(milliseconds / portTICK_PERIOD_MS);
}

void lcd_digitalWrite(unsigned short int pin, unsigned char value)
{
    gpio_set_level(pin, value);
}

void lcd_spiWrite(unsigned char *buffer, size_t length)
{
    esp_err_t ret;
    //esp32的spi发送以一个一个事务来组织。
    spi_transaction_t t = {0};
    if (length == 0)
        return;            // no need to send anything
    t.length = length * 8; // Len is in bytes, transaction length is in bits.
    t.tx_buffer = buffer;  // Data
    // ret = spi_device_polling_transmit(spi, &t); // 这样是同步发送
    //放入队列
    ret = spi_device_queue_trans(spi, &t, portMAX_DELAY); // Transmit!

    assert(ret == ESP_OK); // Should have had no issues.
}
/* ################## END - HAVE TO BE IMPLEMENTED - END #################### */

void task1(void *pvParam)
{
    esp_err_t ret;
    // esp32 spi configuration
    spi_bus_config_t buscfg = {.miso_io_num = PIN_NUM_MISO,
                               .mosi_io_num = PIN_NUM_MOSI,
                               .sclk_io_num = PIN_NUM_CLK,
                               .quadwp_io_num = -1, // unused
                               .quadhd_io_num = -1, // unused
                               .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 1024};  // 系统会检查每个spi事务发送的大小，超过这个会抛异常

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 60 * 1000 * 1000, // Clock out at 10 MHz
        .mode = 0,                          // SPI mode 0
        .spics_io_num = PIN_NUM_CS,         // CS pin
        .queue_size = 7,                    // We want to be able to queue 7 transactions at a time 
    };

    // Initialize the SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    // Attach the LCD to the SPI bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);

    //////////////////////////////////////////////////////
    lcd_ptr_t my_lcd_settings = NULL;

    //根据屏幕实际分辨率修改
    my_lcd_settings = lcd_createSettings(
        128,        /* width */
        160,        /* height */
        0,          /* width_offset */
        0,          /* height_offset */
        PIN_NUM_DC, /* Wiring Pi pin numbering */
        PIN_NUM_RST);

    /* lcd_createSettings will return NULL if memory runs out */
    assert(my_lcd_settings);

    /* This is where you set the settings as active for the library.
     * The library will use them to handle the driver.
     * You can swap it at any time for a different display module.
     * If set to NULL, library functions will return LCD_FAIL.
     */
    lcd_setSettingsActive(my_lcd_settings);

    /* Display initialization. It HAVE TO be done for each display separately.
     * The library will do this for the appropriate display module,
     * based on the active settings.
     */
    lcd_initialize();

    /* To start drawing, you HAVE TO:
     * step 1: turn off sleep mode
     * step 2: set Memory Access Control  - required by lcd_createSettings()
     * step 3: set Interface Pixel Format - required by lcd_createSettings()
     * step 4: turn on the display
     * After that, you can draw.
     *
     * It is best to make the optional settings between steps 1 and 4.
     */

    /* turn off sleep mode; required to draw */
    lcd_setSleepMode(LCD_SLEEP_OUT);

    /* set Memory Access Control; refresh - required by lcd_createSettings() */
    lcd_setMemoryAccessControl(LCD_MADCTL_DEFAULT);

    /* set Interface Pixel Format; refresh - required by lcd_createSettings() */
    lcd_setInterfacePixelFormat(LCD_PIXEL_FORMAT_565);

    /* set Predefined Gamma; optional reset */
    lcd_setGammaPredefined(LCD_GAMMA_PREDEFINED_3);

    /* set Display Inversion; optional reset */
    lcd_setDisplayInversion(LCD_INVERSION_OFF);

    /* set Tearing Effect Line; optional reset */
    lcd_setTearingEffectLine(LCD_TEARING_OFF);

    /* turn on the display; required to draw */
    lcd_setDisplayMode(LCD_DISPLAY_ON);
    printf("!!!lcd should  already init!!!\n");
    /////////////////////////////////////////////////////

    while (1)
    {
        unsigned short int width, height;

        /* get width and height */
        width = lcd_settings->width;
        height = lcd_settings->height;

        bool flag = 1;

        unsigned char buf[2 * 128 * 160] = {0};

        while (1)
        {
            /* set window position */
            lcd_setWindowPosition(0, 0, width - 1, height - 1);

            /* activate memory write */
            lcd_activateMemoryWrite();

            for (size_t i = 0; i < sizeof(buf) / 2; i++)
            {

                ((uint16_t *)(buf))[i] = ST7735_BLACK;

                //    buf[i*2]=(ST7735_RED>>8)&0x00ff;
                //     buf[i*2+1]=ST7735_RED&0x00FF;
                //   r->b    g->r    b->g
            }

            lcd_framebuffer_send(buf, sizeof(buf), INT_MAX);
            flag = !flag;
            vTaskDelay(16 / portTICK_PERIOD_MS);
        }
    }
}

void status_task(void *param)
{
    char *pbuffer = (char *)malloc(2048);
    memset(pbuffer, 0x0, 2048);
    while (1)
    {
        printf("-------------------- heap:%u --------------------------\r\n", esp_get_free_heap_size());
        vTaskList(pbuffer);
        printf("%s", pbuffer);
        printf("----------------------------------------------\r\n");
        vTaskDelay(3000 / portTICK_RATE_MS);
    }
    free(pbuffer);
}

void app_main(void)
{
    TaskHandle_t taskHandle;

    xTaskCreate(task1, "TASK1", 50 * 1024, NULL, 1, &taskHandle);
    xTaskCreate(status_task, "status_task", 4 * 1024, NULL, 5, NULL);

    while (1)
    {
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}