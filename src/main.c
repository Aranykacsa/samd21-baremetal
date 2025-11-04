#include "clock.h"
#include "samd21.h"
#include "system_samd21.h"
#include "uart.h"
#include "variables.h"
#include <stdint.h>
#include <stdio.h>

// === Main ===
int main(void) {
  SystemInit();
  SysTick_Config(SystemCoreClock / 1000);
  uart_init(&uart_s2);
  delay_ms(100);

  char buffer[10];
  int index = 0;
  uart_send_string(&uart_s2, "radio get mod\r\n");

  while (1) {
    // uart_write(&uart_s2, buffer[index++]);
    uint8_t c = uart_read(&uart_s2); // wait for a byte
    buffer[index++] = c;
    if (c == '\n')
      printf("Received: %s\r\n", buffer);

    // delay_ms(100);
  }
}
