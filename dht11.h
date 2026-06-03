#ifndef DHT11_H
#define DHT11_H

#include "stm32f10x.h"

void DHT11_Init(void);
int  DHT11_Read(uint8_t *temperature, uint8_t *humidity);

#endif