#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "system_samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"

int main(void) {

  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);

  printf("INIT\r\n");

  uart_init(&uart_s2);

  delay_ms(3000);
 
  uart_write_string(&uart_s2, "\r\n");
  char buf[64];
  size_t n = uart_read_string_blocking(&uart_s2, buf, 64, 5000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   
  } else {
      printf("No loopback!\n");
  }
  uart_write_string(&uart_s2, "radio get freq\r\n");

  n = uart_read_string_blocking(&uart_s2, buf, 64, 5000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   
  } else {
      printf("No loopback!\n");
  }

  uart_write_string(&uart_s2, "radio set freq 868100000\r\n");

  n = uart_read_string_blocking(&uart_s2, buf, 64, 5000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);
  } else {
      printf("No loopback!\n");
  }
}