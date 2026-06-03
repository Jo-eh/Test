#ifndef LCD_I2C_H
#define LCD_I2C_H

#include "stm32f10x.h"

#define LCD_I2C_ADDR   (0x27 << 1)
#define LCD_BACKLIGHT  0x08
#define LCD_EN         0x04
#define LCD_RS         0x01

void I2C_Config(void);
void LCD_Init(void);
void LCD_SetCursor(uint8_t col, uint8_t row);
void LCD_Print(char *str);
void LCD_Clear(void);

#endif