#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "system_samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"
#include "board.h"

int main(void) {

  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);

  printf("INIT\r\n");
  setup_board();
 
  
}