// samd21_aht20_sercom3.c
#include "samd21.h"
#include "system_samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "uart.h"

int main(void) {

  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);     // 1 ms tick

  /* (Optional) UART for printf */
  uart_init(SERCOM5, 115200, 10, 1, 11, 3, PORT_PMUX_PMUXE_D_Val);
  setvbuf(stdout, NULL, _IONBF, 0);
}
