// samd21_aht20_sercom3.c
#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "system_samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"

int main(void) {

  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);     // 1 ms tick

  printf("INIT\r\n");

  uart_init(SERCOM2, 115200, PIN_SERIAL1_TX, PAD_SERIAL1_TX, PIN_SERIAL1_RX, PAD_SERIAL1_RX, 0);

  delay_ms(3000);
 
  uart_write_string(SERCOM2, "\r\n");
  char buf[64];
  size_t n = uart_read_string_blocking(SERCOM2, buf, sizeof(buf), 1000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   // semihosting console
  } else {
      printf("No loopback!\n");
  }
  uart_write_string(SERCOM2, "radio get freq\r\n");

  n = uart_read_string_blocking(SERCOM2, buf, sizeof(buf), 1000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   // semihosting console
  } else {
      printf("No loopback!\n");
  }

  uart_write_string(SERCOM2, "radio set freq 868100000\r\n");

  n = uart_read_string_blocking(SERCOM2, buf, sizeof(buf), 1000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   // semihosting console
  } else {
      printf("No loopback!\n");
  }
}