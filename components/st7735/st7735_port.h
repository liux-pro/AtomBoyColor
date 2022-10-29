#include <stdint.h>

#define ST7735_BLACK 0x0000
#define ST7735_BLUE 0x001F
#define ST7735_RED 0xF800
#define ST7735_GREEN 0x07E0

#define LCD_WIDTH 128
#define LCD_HEIGHT 160


uint8_t * lcd_get_buffer();
void lcd_fast_init();
void lcd_refresh();
void lcd_fast_refresh();
void lcd_wait_busy();