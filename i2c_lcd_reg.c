#include "i2c_lcd_reg.h"

/* ===========================================================================
 * 1. DELAY FUNCTIONS
 *    Uses DWT cycle counter for accurate microsecond timing.
 *    Required by HD44780 for command settling times.
 * ===========================================================================
 */
void delay_us(uint32_t us)
{
    /* Enable DWT if not already running */
    if (!(CoreDebug->DEMCR & CoreDebug_DEMCR_TRCENA_Msk)) {
        CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
        DWT->CYCCNT = 0;
        DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
    }
    uint32_t start = DWT->CYCCNT;
    /* SystemCoreClock is typically 72 MHz for STM32F1 */
    uint32_t ticks = (SystemCoreClock / 1000000UL) * us;
    while ((DWT->CYCCNT - start) < ticks);
}

void delay_ms(uint32_t ms)
{
    while (ms--) delay_us(1000);
}

/* ===========================================================================
 * 2. I2C1 REGISTER-LEVEL CONFIGURATION
 *    PB6 = SCL, PB7 = SDA — Alternate Function Open-Drain
 *    APB1 = 36 MHz, Standard Mode 100 kHz
 * ===========================================================================
 */
void I2C_Config(void)
{
    /* --- Clock Enable --- */
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    /* Enable GPIOB and Alternate Function IO clocks for STM32F1 */
    RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_AFIOEN;
    __DSB();  /* Ensure clock is active before register access */

    /* --- GPIO Configuration for PB6 (SCL) and PB7 (SDA) --- */

    /* F1 uses CRL/CRH for GPIO mode. PB6 and PB7 are in CRL.
     * Mode: 50 MHz Output (MODE = 11)
     * Config: Alternate Function Open-Drain (CNF = 11)
     * Together: 0xF for each pin
     */
    GPIOB->CRL &= ~(0xFFUL << 24); /* Clear PB6 and PB7 bits (bits 24 to 31) */
    GPIOB->CRL |=  (0xFFUL << 24); /* Set PB6 and PB7 to AF OD 50MHz */

    /* --- I2C1 Peripheral Reset & Configure --- */
    I2C1->CR1 |=  I2C_CR1_SWRST;
    I2C1->CR1 &= ~I2C_CR1_SWRST;

    /* APB1 peripheral clock frequency = 36 MHz (System = 72MHz, APB1 = /2) */
    I2C1->CR2 = 36;

    /*
     * Standard Mode (Sm) 100 kHz:
     *   CCR = Fpclk1 / (2 * Fscl) = 36,000,000 / (2 * 100,000) = 180
     */
    I2C1->CCR = 180;

    /*
     * Maximum rise time:
     *   TRISE = (Tr_max * Fpclk1) + 1 = (1000ns * 36MHz) + 1 = 37
     */
    I2C1->TRISE = 37;

    /* Enable I2C peripheral */
    I2C1->CR1 |= I2C_CR1_PE;
}

/* ===========================================================================
 * 3. LOW-LEVEL I2C PROTOCOL
 * ===========================================================================
 */
void I2C_Start(void)
{
    I2C1->CR1 |= I2C_CR1_START;
    while (!(I2C1->SR1 & I2C_SR1_SB));
}

void I2C_Write(uint8_t data)
{
    while (!(I2C1->SR1 & I2C_SR1_TXE));
    I2C1->DR = data;
    while (!(I2C1->SR1 & I2C_SR1_BTF));
}

void I2C_Address(uint8_t Address)
{
    I2C1->DR = Address;
    while (!(I2C1->SR1 & I2C_SR1_ADDR));
    (void)I2C1->SR2;   /* Clear ADDR flag */
}

void I2C_Stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    /* Brief delay to ensure STOP condition completes on the bus */
    delay_us(10);
}

/* ===========================================================================
 * 4. LCD INTERNAL HELPER — Send One Nibble with Correct EN Pulse
 *
 *  BUG FIX #1: HD44780 requires EN=0 ? EN=1 ? EN=0 (3 writes per nibble).
 *              Original code only sent EN=1 ? EN=0 (data not stable on rise).
 *
 *  Each nibble requires one full I2C transaction:
 *    [START] [ADDR] [data EN=0] [data EN=1] [data EN=0] [STOP]
 * ===========================================================================
 */
static void lcd_send_nibble(uint8_t nibble, uint8_t flags)
{
    /* nibble must already be in the upper 4 bits (D7..D4 of PCF8574) */
    uint8_t d_lo = nibble | flags;            /* EN = 0  (data setup)    */
    uint8_t d_hi = nibble | flags | LCD_EN;   /* EN = 1  (latch rising)  */

    I2C_Start();
    I2C_Address(SLAVE_ADDRESS_LCD);
    I2C_Write(d_lo);    /* 1) Data stable, EN low                        */
    I2C_Write(d_hi);    /* 2) EN high — HD44780 latches on rising edge    */
    I2C_Write(d_lo);    /* 3) EN low  — completes the pulse               */
    I2C_Stop();
    delay_us(50);       /* Minimum EN cycle time = 37 µs per datasheet    */
}

/* ===========================================================================
 * 5. PUBLIC LCD API
 * ===========================================================================
 */

/**
 * @brief Send a command byte to LCD (RS = 0).
 *        Sends upper nibble first, then lower nibble.
 */
void lcd_send_cmd(char cmd)
{
    uint8_t upper = (uint8_t)cmd & 0xF0;          /* D7-D4, upper nibble */
    uint8_t lower = ((uint8_t)cmd << 4) & 0xF0;   /* D7-D4, lower nibble */

    lcd_send_nibble(upper, LCD_BL);           /* RS=0, RW=0, BL=1 */
    lcd_send_nibble(lower, LCD_BL);
}

/**
 * @brief Send a data byte to LCD (RS = 1).
 *        Sends upper nibble first, then lower nibble.
 */
void lcd_send_data(char data)
{
    uint8_t upper = (uint8_t)data & 0xF0;
    uint8_t lower = ((uint8_t)data << 4) & 0xF0;

    lcd_send_nibble(upper, LCD_BL | LCD_RS);  /* RS=1, RW=0, BL=1 */
    lcd_send_nibble(lower, LCD_BL | LCD_RS);
}

/**
 * @brief Send null-terminated string to LCD at current cursor position.
 */
void lcd_send_string(char *str)
{
    while (*str) {
        lcd_send_data(*str++);
    }
}

/**
 * @brief Move cursor to specified row and column (0-indexed).
 *        Row 0 = line 1 (DDRAM 0x00), Row 1 = line 2 (DDRAM 0x40)
 *
 *  BUG FIX #5: Function was declared but never implemented.
 */
void lcd_put_cur(int row, int col)
{
    uint8_t address;
    switch (row) {
        case 0:  address = 0x00 + (uint8_t)col; break;
        case 1:  address = 0x40 + (uint8_t)col; break;
        default: address = 0x00; break;
    }
    lcd_send_cmd(0x80 | address);  /* Set DDRAM Address command */
}

/**
 * @brief Clear display and return cursor home.
 *        Adds mandatory 2ms delay (command takes 1.52ms to execute).
 */
void lcd_clear(void)
{
    lcd_send_cmd(0x01);
    delay_ms(2);   /* ? BUG FIX #4: Clear Display needs >1.52ms */
}

/* ===========================================================================
 * 6. LCD INITIALIZATION — Full HD44780 Power-On Sequence
 *
 *  BUG FIX #2: The original code skipped the mandatory 8-bit mode init
 *              sequence. Going straight to 0x28 leaves the LCD in an
 *              undefined state on power-up.
 *
 *  Correct sequence (from HD44780 datasheet, page 45):
 *    1. Power on wait >40ms
 *    2. Send 0x30 (Function Set, 8-bit) — wait >4.1ms
 *    3. Send 0x30 again               — wait >100µs
 *    4. Send 0x30 again               — wait >37µs
 *    5. Send 0x20 (switch to 4-bit)   — wait >37µs
 *    6. Now in 4-bit mode: configure normally
 * ===========================================================================
 */
void lcd_init(void)
{
    I2C_Config();

    /* Wait for LCD power supply to stabilise (>40ms required) */
    delay_ms(50);

    /* ----------------------------------------------------------------
     * Step 1-4: Force 8-bit mode three times, then switch to 4-bit.
     * These are single-nibble writes (not full byte commands).
     * We only send the upper nibble, hence (0x30) with no lower nibble.
     * ---------------------------------------------------------------- */

    /* First 0x30: wake up, wait >4.1ms */
    lcd_send_nibble(0x30, LCD_BL);
    delay_ms(5);

    /* Second 0x30: wait >100µs */
    lcd_send_nibble(0x30, LCD_BL);
    delay_us(150);

    /* Third 0x30: wait >37µs */
    lcd_send_nibble(0x30, LCD_BL);
    delay_us(50);

    /* Switch to 4-bit mode: send 0x20 as single nibble */
    lcd_send_nibble(0x20, LCD_BL);
    delay_us(50);

    /* ----------------------------------------------------------------
     * Step 5+: All subsequent commands are full 4-bit mode (2 nibbles)
     * ---------------------------------------------------------------- */

    /* Function Set: 4-bit, 2 lines, 5x8 dot font */
    lcd_send_cmd(0x28);
    delay_us(50);

    /* Display OFF */
    lcd_send_cmd(0x08);
    delay_us(50);

    /* Clear Display — needs >1.52ms settling time */
    lcd_send_cmd(0x01);
    delay_ms(2);             /* ? BUG FIX #4 */

    /* Entry Mode Set: increment cursor, no display shift */
    lcd_send_cmd(0x06);
    delay_us(50);

    /* Display ON, Cursor OFF, Blink OFF */
    lcd_send_cmd(0x0C);
    delay_us(50);
}
