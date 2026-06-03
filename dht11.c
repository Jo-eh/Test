#include "dht11.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_rcc.h"

#define DHT11_PORT  GPIOB
#define DHT11_PIN   GPIO_Pin_8

// -------------------------------------------------------------
// SUBROUTINE: Microsecond Delay
// -------------------------------------------------------------
static void Delay_us(uint32_t us) {
    uint32_t count = us * 6;   // Calibrated for ~72MHz
    while (count--) {
        __NOP();
    }
}

// -------------------------------------------------------------
// SUBROUTINE: Set DHT11 Pin as Output
// -------------------------------------------------------------
static void DHT11_SetOutput(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin   = DHT11_PIN;
    GPIO_InitStruct.GPIO_Mode  = GPIO_Mode_Out_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// -------------------------------------------------------------
// SUBROUTINE: Set DHT11 Pin as Input
// -------------------------------------------------------------
static void DHT11_SetInput(void) {
    GPIO_InitTypeDef GPIO_InitStruct;
    GPIO_InitStruct.GPIO_Pin  = DHT11_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

// -------------------------------------------------------------
// SUBROUTINE: DHT11 Init
// -------------------------------------------------------------
void DHT11_Init(void) {
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    DHT11_SetOutput();
    GPIO_SetBits(DHT11_PORT, DHT11_PIN);   // Idle HIGH
}

// -------------------------------------------------------------
// SUBROUTINE: DHT11 Read
// Returns 0 = success, -1 = error/timeout/checksum fail
// -------------------------------------------------------------
int DHT11_Read(uint8_t *temperature, uint8_t *humidity) {
    uint8_t  data[5] = {0};
    uint32_t timeout;

    // Send start signal: LOW for 18ms, then HIGH
    DHT11_SetOutput();
    GPIO_ResetBits(DHT11_PORT, DHT11_PIN);
    Delay_us(18000);
    GPIO_SetBits(DHT11_PORT, DHT11_PIN);
    Delay_us(30);

    // Switch to input, wait for sensor response
    DHT11_SetInput();

    // Wait for sensor to pull LOW (~80us)
    timeout = 10000;
    while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == Bit_SET)
        if (--timeout == 0) return -1;

    // Wait for sensor to pull HIGH (~80us)
    timeout = 10000;
    while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == Bit_RESET)
        if (--timeout == 0) return -1;

    // Wait for HIGH pulse to end
    timeout = 10000;
    while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == Bit_SET)
        if (--timeout == 0) return -1;

    // Read 40 bits (5 bytes)
    for (int i = 0; i < 5; i++) {
        for (int bit = 7; bit >= 0; bit--) {

            // Wait for pin HIGH (start of bit)
            timeout = 10000;
            while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == Bit_RESET)
                if (--timeout == 0) return -1;

            // Sample after 40us: HIGH = '1', LOW = '0'
            Delay_us(40);
            if (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == Bit_SET) {
                data[i] |= (1 << bit);

                // Wait for pin to go LOW before next bit
                timeout = 10000;
                while (GPIO_ReadInputDataBit(DHT11_PORT, DHT11_PIN) == Bit_SET)
                    if (--timeout == 0) return -1;
            }
        }
    }

    // Verify checksum
    if (data[4] != ((data[0] + data[1] + data[2] + data[3]) & 0xFF))
        return -1;

    *humidity    = data[0];
    *temperature = data[2];
    return 0;
}