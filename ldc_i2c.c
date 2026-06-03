#include "lcd_i2c.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

// -------------------------------------------------------------
// SUBROUTINE: Send One Byte Over I2C to LCD Backpack
// -------------------------------------------------------------
static void I2C_WriteByte(uint8_t data) {
    uint32_t timeout;

    timeout = 10000;
    I2C_GenerateSTART(I2C1, ENABLE);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
        if (--timeout == 0) return;

    timeout = 10000;
    I2C_Send7bitAddress(I2C1, LCD_I2C_ADDR, I2C_Direction_Transmitter);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
        if (--timeout == 0) return;

    timeout = 10000;
    I2C_SendData(I2C1, data);
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        if (--timeout == 0) return;

    I2C_GenerateSTOP(I2C1, ENABLE);
}

// -------------------------------------------------------------
// SUBROUTINE: Pulse LCD Enable Pin (latches data into LCD)
// -------------------------------------------------------------
static void LCD_PulseEnable(uint8_t data) {
    I2C_WriteByte(data | LCD_EN  | LCD_BACKLIGHT);
    I2C_WriteByte(data & ~LCD_EN | LCD_BACKLIGHT);
}

// -------------------------------------------------------------
// SUBROUTINE: Send High Nibble (4-bit mode)
// -------------------------------------------------------------
static void LCD_SendNibble(uint8_t nibble, uint8_t mode) {
    uint8_t data = (nibble & 0xF0) | mode | LCD_BACKLIGHT;
    LCD_PulseEnable(data);
}

// -------------------------------------------------------------
// SUBROUTINE: Send Full Byte as Two Nibbles
// -------------------------------------------------------------
static void LCD_SendByte(uint8_t byte, uint8_t mode) {
    LCD_SendNibble( byte & 0xF0,        mode);   // High nibble
    LCD_SendNibble((byte << 4) & 0xF0,  mode);   // Low nibble
}

// -------------------------------------------------------------
// SUBROUTINE: Send LCD Command
// -------------------------------------------------------------
static void LCD_Command(uint8_t cmd) {
    LCD_SendByte(cmd, 0x00);   // RS = 0
}

// -------------------------------------------------------------
// SUBROUTINE: Send LCD Data Character
// -------------------------------------------------------------
static void LCD_DataWrite(uint8_t data) {
    LCD_SendByte(data, LCD_RS);   // RS = 1
}

// -------------------------------------------------------------
// SUBROUTINE: Delay for LCD Timing
// -------------------------------------------------------------
static void LCD_Delay_us(uint32_t us) {
    uint32_t count = us * 6;
    while (count--) __NOP();
}

// -------------------------------------------------------------
// SUBROUTINE: I2C1 Peripheral Init (PB6=SCL, PB7=SDA)
// -------------------------------------------------------------
void I2C_Config(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    I2C_InitTypeDef  I2C_InitStruct;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    I2C_InitStruct.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitStruct.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitStruct.I2C_OwnAddress1         = 0x00;
    I2C_InitStruct.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitStruct.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStruct.I2C_ClockSpeed          = 100000;

    I2C_Init(I2C1, &I2C_InitStruct);
    I2C_Cmd(I2C1, ENABLE);
}

// -------------------------------------------------------------
// SUBROUTINE: LCD Full Initialization Sequence
// -------------------------------------------------------------
void LCD_Init(void) {
    LCD_Delay_us(50000);

    LCD_SendNibble(0x30, 0x00); LCD_Delay_us(5000);
    LCD_SendNibble(0x30, 0x00); LCD_Delay_us(200);
    LCD_SendNibble(0x30, 0x00); LCD_Delay_us(200);
    LCD_SendNibble(0x20, 0x00); LCD_Delay_us(200);   // 4-bit mode

    LCD_Command(0x28);          // 2 lines, 5x8 font
    LCD_Command(0x0C);          // Display ON, cursor OFF
    LCD_Command(0x06);          // Auto-increment cursor
    LCD_Command(0x01);          // Clear display
    LCD_Delay_us(2000);
}

// -------------------------------------------------------------
// SUBROUTINE: Set LCD Cursor Position
// -------------------------------------------------------------
void LCD_SetCursor(uint8_t col, uint8_t row) {
    uint8_t rowOffset[] = {0x00, 0x40};
    LCD_Command(0x80 | (col + rowOffset[row]));
}

// -------------------------------------------------------------
// SUBROUTINE: Print String to LCD
// -------------------------------------------------------------
void LCD_Print(char *str) {
    while (*str) {
        LCD_DataWrite((uint8_t)(*str));
        str++;
    }
}

// -------------------------------------------------------------
// SUBROUTINE: Clear LCD Screen
// -------------------------------------------------------------
void LCD_Clear(void) {
    LCD_Command(0x01);
    LCD_Delay_us(2000);
}