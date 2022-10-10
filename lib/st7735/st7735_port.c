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

/*
用于适配st7735的驱动
下面三个函数是正常的移植，采用同步的方式发送spi
为了高速刷屏，提供额外函数，其内部，开启spi队列，异步发送，同时用spi事务回调的方法控制d/c线
*/

#define LCD_HOST SPI2_HOST

#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI 3
#define PIN_NUM_CLK 2
#define PIN_NUM_CS 7

#define PIN_NUM_BL 11
#define PIN_NUM_DC 8
#define PIN_NUM_RST 10

spi_device_handle_t spi;
// 显存
unsigned char buf[LCD_WIDTH * LCD_HEIGHT * 2] = {0};

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
    // esp32的spi发送以一个一个事务来组织。
    spi_transaction_t t = {0};
    if (length == 0)
        return;                                 // no need to send anything
    t.length = length * 8;                      // Len is in bytes, transaction length is in bits.
    t.tx_buffer = buffer;                       // Data
    ret = spi_device_polling_transmit(spi, &t); // 这样是同步发送
    //放入队列
    // ret = spi_device_queue_trans(spi, &t, portMAX_DELAY); // Transmit!

    assert(ret == ESP_OK); // Should have had no issues.
}
/* ################## END - HAVE TO BE IMPLEMENTED - END #################### */

static void lcd_spi_init()
{
    esp_err_t ret;
    // esp32 spi configuration
    spi_bus_config_t buscfg = {.miso_io_num = PIN_NUM_MISO,
                               .mosi_io_num = PIN_NUM_MOSI,
                               .sclk_io_num = PIN_NUM_CLK,
                               .quadwp_io_num = -1,                                   // unused
                               .quadhd_io_num = -1,                                   // unused
                               .max_transfer_sz = LCD_WIDTH * LCD_HEIGHT * 2 + 1024}; // 系统会检查每个spi事务发送的大小，超过这个会抛异常

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 40 * 1000 * 1000, // Clock out at 10 MHz
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

    // 初始化gpio
    gpio_set_direction(PIN_NUM_DC, GPIO_MODE_OUTPUT);
    gpio_set_direction(PIN_NUM_RST, GPIO_MODE_OUTPUT);
}

void lcd_fast_init()
{
    lcd_spi_init();

    lcd_ptr_t my_lcd_settings = NULL;

    //根据屏幕实际分辨率修改
    //这个函数改了以下，避免使用动态内存分配。同时也用不到free
    my_lcd_settings = lcd_createSettings(
        LCD_WIDTH,  /* width */
        LCD_HEIGHT, /* height */
        0,          /* width_offset */
        0,          /* height_offset */
        PIN_NUM_DC, /* Wiring Pi pin numbering */
        PIN_NUM_RST);

    /* lcd_createSettings will return NULL if memory runs out */
    assert(my_lcd_settings);

    lcd_setSettingsActive(my_lcd_settings);
    lcd_initialize();
    lcd_setSleepMode(LCD_SLEEP_OUT);
    lcd_setMemoryAccessControl(LCD_MADCTL_DEFAULT);
    lcd_setInterfacePixelFormat(LCD_PIXEL_FORMAT_565);
    lcd_setGammaPredefined(LCD_GAMMA_PREDEFINED_3);
    lcd_setDisplayInversion(LCD_INVERSION_OFF);
    lcd_setTearingEffectLine(LCD_TEARING_OFF);
    lcd_setDisplayMode(LCD_DISPLAY_ON);
}

uint8_t *lcd_get_buffer()
{
    return buf;
}

void lcd_send_buffer()
{
    /* set window position */
    lcd_setWindowPosition(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    /* activate memory write */
    lcd_activateMemoryWrite();
    lcd_framebuffer_send(buf, LCD_WIDTH * LCD_HEIGHT * 2, 1024);
}