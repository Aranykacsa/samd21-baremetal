#include "clock.h"
#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "system_samd21.h"

volatile uint32_t g_ms = 0;
void SysTick_Handler(void) { g_ms++; }
void delay_ms(uint32_t ms) {
  uint32_t start = g_ms;
  while ((g_ms - start) < ms) { __NOP(); }
}