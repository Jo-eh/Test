#ifndef I2C_LCD_REG_H
#define I2C_LCD_REG_H

/*
 * ============================================================================
 * I2C LCD Driver — STM32F103C8 Blue Pill
 * ----------------------------------------------------------------------------
 * Hardware : HD44780 + PCF8574 I2C backpack
 * I2C Pins : PB6 = SCL  |  PB7 = SDA   (I2C1, no remap needed)
 * APB1 Clk : 36 MHz  (System = 72 MHz, APB1 prescaler = /2)
 * I2C Speed: Standard Mode 100 kHz
 * ============================================================================
 */

#include "stm32f10x.h"          /* Blue Pill CMSIS header — replaces stm32f4xx.h */

/* ---------------------------------------------------------------------------
 * LCD I2C Address
 *   PCF8574 default A2:A1:A0 = 0:0:0 -> 7-bit address = 0x27
 *   Left-shifted for write: 0x27 << 1 = 0x4E
 *   If your module uses PCF8574A chip, default 7-bit = 0x3F -> 0x7E
 * --------------------------------------------------------------------------- */
#define SLAVE_ADDRESS_LCD   0x4E

/* ---------------------------------------------------------------------------
 * PCF8574 Output Bit Mapping
 *   P7  P6  P5  P4  | P3   P2   P1   P0
 *   D7  D6  D5  D4  | BL   EN   RW   RS
 * --------------------------------------------------------------------------- */
#define LCD_RS   0x01           /* Register Select : 0 = Command, 1 = Data  */
#define LCD_RW   0x02           /* Read/Write      : 0 = Write,  1 = Read   */
#define LCD_EN   0x04           /* Enable pulse pin                          */
#define LCD_BL   0x08           /* Backlight ON                              */

/* ---------------------------------------------------------------------------
 * Function Prototypes
 * --------------------------------------------------------------------------- */

/* I2C hardware layer */
void I2C_Config(void);
void I2C_Start(void);
void I2C_Write(uint8_t data);
void I2C_Address(uint8_t Address);
void I2C_Stop(void);

/* Timing utilities */
void delay_us(uint32_t us);
void delay_ms(uint32_t ms);

/* LCD public API */
void lcd_init(void);
void lcd_send_cmd(char cmd);
void lcd_send_data(char data);
void lcd_send_string(char *str);
void lcd_put_cur(int row, int col);
void lcd_clear(void);

#endif /* I2C_LCD_REG_H */
