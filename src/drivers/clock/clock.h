#ifndef CLOCK_H
#define CLOCK_H

#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "system_samd21.h"

/* ---------- accurate 1 ms tick ---------- */
extern volatile uint32_t g_ms;
void SysTick_Handler(void);
void delay_ms(uint32_t ms);

#endif