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

#define SPI_TRANSACTION_POOL_SIZE 20

spi_device_handle_t spi;
// 显存
unsigned char buf[LCD_WIDTH * LCD_HEIGHT * 2] = {0};

//池化spi_transaction
// spi快速发送是基于spi_transaction的
spi_transaction_t spi_transaction_pool[SPI_TRANSACTION_POOL_SIZE] = {0};

spi_transaction_t get_spi_transaction()
{
    static uint8_t i = 0;
    i++;
    if (i > SPI_TRANSACTION_POOL_SIZE)
    {
        i = 0;
    }
    memset(&spi_transaction_pool[i], 0, sizeof(spi_transaction_t)); // Zero out the transaction
    return spi_transaction_pool[i];
}

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
        return;            // no need to send anything
    t.length = length * 8; // Len is in bytes, transaction length is in bits.
    t.tx_buffer = buffer;  // Data
    t.user = (void *)0xff;
    ret = spi_device_transmit(spi, &t); // 这样是同步发送
    // ret = spi_device_queue_trans(spi, &t, portMAX_DELAY); // Transmit!
    //放入队列
    // ret = spi_device_queue_trans(spi, &t, portMAX_DELAY); // Transmit!

    assert(ret == ESP_OK); // Should have had no issues.
}
/* ################## END - HAVE TO BE IMPLEMENTED - END #################### */

/* Send a command to the LCD. Uses spi_device_polling_transmit, which waits
 * until the transfer is complete.
 *
 * Since command transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void st7735_cmd(const uint8_t cmd)
{
    esp_err_t ret;
    spi_transaction_t t = get_spi_transaction();
    t.length = 8;       // Command is 8 bits
    t.tx_data[0] = cmd; // The data is the cmd itself
    t.flags = SPI_TRANS_USE_TXDATA;
    t.user = (void *)0; // D/C needs to be set to 0
    // ret = spi_device_queue_trans(spi, &t,portMAX_DELAY); // Transmit!
    ret = spi_device_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);              // Should have had no issues.
}

/* Send data to the LCD. Uses spi_device_polling_transmit, which waits until the
 * transfer is complete.
 *
 * Since data transactions are usually small, they are handled in polling
 * mode for higher speed. The overhead of interrupt transactions is more than
 * just waiting for the transaction to complete.
 */
void st7735_data(const uint8_t *data, int len)
{
    esp_err_t ret;
    spi_transaction_t t = get_spi_transaction();
    if (len == 0)
        return;                         // no need to send anything
    t.length = len * 8;                 // Len is in bytes, transaction length is in bits.
    t.tx_buffer = data;                 // Data
    t.user = (void *)1;                 // D/C needs to be set to 1
    // ret = spi_device_queue_trans(spi, &t,portMAX_DELAY); // Transmit!
    ret = spi_device_transmit(spi, &t); // Transmit!
    assert(ret == ESP_OK);              // Should have had no issues.
}

// void lcd_wait_busy()
// {
//     spi_transaction_t *rtrans;

//     /* 查询并阻塞等待所有传输结束 */
//     for (int i = 0; i < SPI_TRANSACTION_POOL_SIZE; i++)
//     {
//         ESP_ERROR_CHECK(
//             spi_device_get_trans_result(spi, &rtrans, 1));
//     }
// }

// This function is called (in irq context!) just before a transmission starts. It will
// set the D/C line to the value indicated in the user field.
void lcd_spi_pre_transfer_callback(spi_transaction_t *t)
{
    if ((int)t->user != 0xff)
    {
        int dc = (int)t->user;
        gpio_set_level(PIN_NUM_DC, dc);
    }
}

//////////////////////////

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
        .clock_speed_hz = SPI_MASTER_FREQ_40M,      // Clock out at 10 MHz
        .mode = 0,                               // SPI mode 0
        .spics_io_num = PIN_NUM_CS,              // CS pin
        .queue_size = SPI_TRANSACTION_POOL_SIZE, // We want to be able to queue 7 transactions at a time
        .pre_cb = lcd_spi_pre_transfer_callback};

    // Initialize the SPI bus
    ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
    ESP_ERROR_CHECK(ret);
    // Attach the LCD to the SPI bus
    ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
    ESP_ERROR_CHECK(ret);

    // 初始化gpio
    gpio_reset_pin(PIN_NUM_DC);
    gpio_reset_pin(PIN_NUM_RST);
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

void lcd_refresh()
{
    /* set window position */
    lcd_setWindowPosition(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    /* activate memory write */
    lcd_activateMemoryWrite();
    //第三个参数是每次最大发送的大小，也就是如果总数据量大，会自动分包。
    //不知道为什么调大后就没法用了？
    //同样的代码封装在一个函数里也没法用，必须调小后才能用。
    //难道是因为freertos的调度，导致大包的发送过程被中断了？
    lcd_framebuffer_send(buf, LCD_WIDTH * LCD_HEIGHT * 2, INT_MAX);
}

static void lcd_setWindowPosition_fast(
    unsigned short int column_start,
    unsigned short int row_start,
    unsigned short int column_end,
    unsigned short int row_end)
{
    static uint8_t LCD_CMD_CASET = 0x2A; /* Column Address Set (pdf v1.4 p128-129) */
    static uint8_t LCD_CMD_RASET = 0x2B; /* Row Address Set (pdf v1.4 p130-131) */

    static unsigned char buffer1[4]; /* four bytes */
    static unsigned char buffer2[4]; /* four bytes */

    /* append offset to variables; after checking the ranges */
    /* width_offset  :: column :: X */
    /* height_offset :: row    :: Y */
    column_start += lcd_settings->width_offset;
    column_end += lcd_settings->width_offset;
    row_start += lcd_settings->height_offset;
    row_end += lcd_settings->height_offset;

    /* write column address; requires 4 bytes of the buffer */
    buffer1[0] = (column_start >> 8) & 0x00FF; /* MSB */ /* =0 for ST7735S */
    buffer1[1] = column_start & 0x00FF;                  /* LSB */
    buffer1[2] = (column_end >> 8) & 0x00FF; /* MSB */   /* =0 for ST7735S */
    buffer1[3] = column_end & 0x00FF;                    /* LSB */
    st7735_cmd(LCD_CMD_CASET);
    st7735_data(buffer1, 4);

    /* write row address; requires 4 bytes of the buffer */
    buffer2[0] = (row_start >> 8) & 0x00FF; /* MSB */ /* =0 for ST7735S */
    buffer2[1] = row_start & 0x00FF;                  /* LSB */
    buffer2[2] = (row_end >> 8) & 0x00FF; /* MSB */   /* =0 for ST7735S */
    buffer2[3] = row_end & 0x00FF;                    /* LSB */
    st7735_cmd(LCD_CMD_RASET);
    st7735_data(buffer2, 4);
}

void lcd_framebuffer_send_fast(
    unsigned char *buffer,
    const size_t length_buffer,
    const size_t length_chunk)
{
    unsigned int i;
    unsigned int chunk_amount;
    unsigned int remainder;

    chunk_amount = length_buffer / length_chunk; /* integer division */
    remainder = length_buffer % length_chunk;

    /* send chunks */
    for (i = 0; i < chunk_amount; i++)
    {

        st7735_data(
            buffer + i * length_chunk,
            length_chunk);
    }
    /* send the remainder, if any */
    if (remainder > 0)
    {

        st7735_data(
            buffer + chunk_amount * length_chunk,
            remainder);
    }
}

void lcd_fast_refresh()
{

    /* set window position */
    lcd_setWindowPosition_fast(0, 0, LCD_WIDTH - 1, LCD_HEIGHT - 1);
    /* activate memory write */
    static uint8_t LCD_CMD_RAMWR = 0x2C; /* Memory Write (pdf v1.4 p132) */
    st7735_cmd(LCD_CMD_RAMWR);
    //第三个参数是每次最大发送的大小，也就是如果总数据量大，会自动分包。
    //不知道为什么调大后就没法用了？
    //同样的代码封装在一个函数里也没法用，必须调小后才能用。
    //难道是因为freertos的调度，导致大包的发送过程被中断了？
    // st7735_data(buf, LCD_WIDTH * LCD_HEIGHT * 2);
    lcd_framebuffer_send_fast(buf, LCD_WIDTH * LCD_HEIGHT * 2, SPI_MAX_DMA_LEN);
}
