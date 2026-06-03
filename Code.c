#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_rcc.h"
#include <stdio.h>

// pin definitions
#define DHT_PORT     GPIOB
#define DHT_PIN      GPIO_Pin_8

#define LED_PORT     GPIOB
#define LED_PIN      GPIO_Pin_1

#define BUZZER_PORT  GPIOB
#define BUZZER_PIN   GPIO_Pin_10

#define LCD_ADDR       (0x27 << 1)   // default PCF8574 address, can change if need
#define LCD_BACKLIGHT  0x08
#define LCD_ENABLE     0x04
#define LCD_RS         0x01

#define I2C_TIMEOUT  100000

// ============================================================
// MEMBER 1 - Timing and GPIO
// ============================================================

volatile uint32_t msTicks = 0;

// INTERRUPT - SysTick fires every 1ms, used for Delay_ms
void SysTick_Handler(void)
{
    msTicks++;
}

// SUBROUTINE - millisecond delay using SysTick counter
void Delay_ms(uint32_t ms)
{
    uint32_t start = msTicks;
    while ((msTicks - start) < ms);
}

void DWT_Init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL   |= DWT_CTRL_CYCCNTENA_Msk;
}

// microsecond delay for DHT22 timing, uses DWT cycle counter
void Delay_us(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t ticks = us * (SystemCoreClock / 1000000);
    while ((DWT->CYCCNT - start) < ticks);
}

void GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStruct.GPIO_Pin   = LED_PIN | BUZZER_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(GPIOB, &GPIO_InitStruct);

    // make sure both are off at start
    GPIO_ResetBits(LED_PORT, LED_PIN);
    GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
}

// ============================================================
// MEMBER 2 - I2C Driver + LCD
// ============================================================

void I2C1_Config(void)
{
    GPIO_InitTypeDef gpio;
    I2C_InitTypeDef  i2c;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;  // SCL = PB6, SDA = PB7
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &gpio);

    I2C_DeInit(I2C1);

    i2c.I2C_ClockSpeed          = 100000;
    i2c.I2C_Mode                = I2C_Mode_I2C;
    i2c.I2C_DutyCycle           = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1         = 0x00;
    i2c.I2C_Ack                 = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

    I2C_Init(I2C1, &i2c);
    I2C_Cmd(I2C1, ENABLE);
}

uint8_t I2C_SendByte(uint8_t addr, uint8_t data)
{
    uint32_t t;

    t = I2C_TIMEOUT;
    while (I2C_GetFlagStatus(I2C1, I2C_FLAG_BUSY))
        if (t-- == 0) return 0;

    I2C_GenerateSTART(I2C1, ENABLE);

    t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_MODE_SELECT))
        if (t-- == 0) return 0;

    I2C_Send7bitAddress(I2C1, addr, I2C_Direction_Transmitter);

    t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
        if (t-- == 0) return 0;

    I2C_SendData(I2C1, data);

    t = I2C_TIMEOUT;
    while (!I2C_CheckEvent(I2C1, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
        if (t-- == 0) return 0;

    I2C_GenerateSTOP(I2C1, ENABLE);

    return 1;
}

// pulse the enable pin so LCD latches the nibble
void LCD_Pulse(uint8_t data)
{
    I2C_SendByte(LCD_ADDR, data | LCD_BACKLIGHT | LCD_ENABLE);
    Delay_us(50);
    I2C_SendByte(LCD_ADDR, data | LCD_BACKLIGHT);
    Delay_us(50);
}

void LCD_WriteNibble(uint8_t value, uint8_t mode)
{
    uint8_t hi = value & 0xF0;
    uint8_t lo = (value << 4) & 0xF0;
    LCD_Pulse(hi | mode);
    LCD_Pulse(lo | mode);
}

void LCD_Cmd(uint8_t cmd)
{
    LCD_WriteNibble(cmd, 0x00);
    Delay_ms(3);
}

void LCD_Char(uint8_t ch)
{
    LCD_WriteNibble(ch, LCD_RS);
}

void LCD_Init(void)
{
    Delay_ms(50);

    // reset sequence - from HD44780 datasheet
    LCD_Pulse(0x30); Delay_ms(5);
    LCD_Pulse(0x30); Delay_ms(5);
    LCD_Pulse(0x30); Delay_ms(2);
    LCD_Pulse(0x20); Delay_ms(2);

    LCD_Cmd(0x28);   // 4-bit mode, 2 lines, 5x8 font
    LCD_Cmd(0x0C);   // display on, no cursor
    LCD_Cmd(0x06);   // auto increment, no shift
    LCD_Cmd(0x01);   // clear display
    Delay_ms(5);
}

void LCD_Clear(void)
{
    LCD_Cmd(0x01);
    Delay_ms(3);
}

void LCD_GotoXY(uint8_t col, uint8_t row)
{
    if (row == 0)
        LCD_Cmd(0x80 | col);
    else
        LCD_Cmd(0xC0 | col);
}

void LCD_Print(char *s)
{
    while (*s)
    {
        LCD_Char(*s);
        s++;
    }
}

// ============================================================
// MEMBER 3 - DHT22 Sensor Driver
// ============================================================

void DHT22_PinOut(void)
{
    GPIO_InitTypeDef DHT_GPIO;

    DHT_GPIO.GPIO_Pin   = DHT_PIN;
    DHT_GPIO.GPIO_Mode  = GPIO_Mode_Out_OD;
    DHT_GPIO.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT_PORT, &DHT_GPIO);
}

void DHT22_PinIn(void)
{
    GPIO_InitTypeDef DHT_GPIO;

    DHT_GPIO.GPIO_Pin   = DHT_PIN;
    DHT_GPIO.GPIO_Mode  = GPIO_Mode_IPU;
    DHT_GPIO.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT_PORT, &DHT_GPIO);
}

// wait for pin to reach target state, return 0 if timeout
uint8_t DHT22_WaitLevel(uint8_t lvl, uint32_t us)
{
    while (GPIO_ReadInputDataBit(DHT_PORT, DHT_PIN) != lvl)
    {
        if (us == 0) return 0;
        us--;
        Delay_us(1);
    }
    return 1;
}

// reads temp (x10) and humidity (x10) from DHT22
// returns 1 if success, 0 if failed
uint8_t DHT22_Read(int16_t *tempOut, uint16_t *humOut)
{
    uint8_t  rawData[5] = {0, 0, 0, 0, 0};
    uint8_t  i;
    uint16_t rawTemp, rawHum;

    // send start pulse
    DHT22_PinOut();
    GPIO_ResetBits(DHT_PORT, DHT_PIN);
    Delay_ms(2);                        // hold LOW for 2ms
    GPIO_SetBits(DHT_PORT, DHT_PIN);
    Delay_us(30);
    DHT22_PinIn();

    // sensor should pull low then high then low as ACK
    if (!DHT22_WaitLevel(0, 100)) return 0;
    if (!DHT22_WaitLevel(1, 100)) return 0;
    if (!DHT22_WaitLevel(0, 100)) return 0;

    for (i = 0; i < 40; i++)
    {
        if (!DHT22_WaitLevel(1, 100)) return 0;

        Delay_us(40);   // if still high after 40us, its a 1 bit

        if (GPIO_ReadInputDataBit(DHT_PORT, DHT_PIN) == Bit_SET)
            rawData[i / 8] |= (1 << (7 - (i % 8)));

        if (!DHT22_WaitLevel(0, 100)) return 0;
    }

    // verify checksum
    // should retry if this fails?
    if (((rawData[0] + rawData[1] + rawData[2] + rawData[3]) & 0xFF) != rawData[4])
        return 0;

    rawHum  = ((uint16_t)rawData[0] << 8) | rawData[1];
    rawTemp = ((uint16_t)(rawData[2] & 0x7F) << 8) | rawData[3];

    *humOut = rawHum;

    if (rawData[2] & 0x80)             // MSB = sign bit for negative temp
        *tempOut = -(int16_t)rawTemp;
    else
        *tempOut = (int16_t)rawTemp;

    return 1;
}

// ============================================================
// MEMBER 4 - Alarm, Display, Main Loop
// ============================================================

void Alarm_On(void)
{
    GPIO_SetBits(LED_PORT, LED_PIN);
    GPIO_SetBits(BUZZER_PORT, BUZZER_PIN);
}

void Alarm_Off(void)
{
    GPIO_ResetBits(LED_PORT, LED_PIN);
    GPIO_ResetBits(BUZZER_PORT, BUZZER_PIN);
}

// blink alarm once (called in loop so it keeps blinking while temp is high)
void Alarm_Blink(void)
{
    Alarm_On();
    Delay_ms(500);
    Alarm_Off();
    Delay_ms(500);
}

void Display_Data(int16_t temp10, uint16_t hum10)
{
    char line[17];
    int  tWhole, tFrac;
    int  hWhole, hFrac;

    tWhole = temp10 / 10;
    tFrac  = temp10 % 10;
    hWhole = hum10 / 10;
    hFrac  = hum10 % 10;

    // negative temp fix - tFrac will also be negative so flip it
    if (tFrac < 0) tFrac = -tFrac;

    LCD_GotoXY(0, 0);
    sprintf(line, "Temp: %d.%d C    ", tWhole, tFrac);
    LCD_Print(line);

    LCD_GotoXY(0, 1);
    sprintf(line, "Humi: %d.%d%%    ", hWhole, hFrac);
    LCD_Print(line);
}

void Display_SensorError(void)
{
    LCD_GotoXY(0, 0);
    LCD_Print("DHT22 Error!    ");
    LCD_GotoXY(0, 1);
    LCD_Print("Recheck wiring  ");
}

int main(void)
{
    int16_t  temp10;
    uint16_t hum10;
    uint8_t  status;

    SystemCoreClockUpdate();
    SysTick_Config(SystemCoreClock / 1000);   // SysTick interrupt every 1ms
    DWT_Init();

    GPIO_Config();
    DHT22_PinIn();
    I2C1_Config();
    LCD_Init();

    LCD_Clear();
    LCD_GotoXY(0, 0);
    LCD_Print("Smart Room Sys");
    LCD_GotoXY(0, 1);
    LCD_Print("Starting up...");
    Delay_ms(2000);
    LCD_Clear();

    while (1)
    {
        status = DHT22_Read(&temp10, &hum10);

        if (status == 1)
        {
            Display_Data(temp10, hum10);

            if (temp10 >= 400)    // 40.0 C and above triggers alarm
                Alarm_Blink();
            else
                Alarm_Off();
        }
        else
        {
            Display_SensorError();
            Alarm_Off();
        }

        Delay_ms(2000);   // DHT22 needs at least 2s between reads
    }
}