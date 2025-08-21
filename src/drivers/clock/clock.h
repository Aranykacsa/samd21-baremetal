#ifndef CLOCK_H
#define CLOCK_H

#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "system_samd21.h"

/* ---------- accurate 1 ms tick ---------- */
void SysTick_Handler(void);
void delay_ms(uint32_t ms);
uint32_t get_uptime(void);

#endif