// samd21_aht20_sercom3.c
#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include "system_samd21.h"
#include "uart.h"
#include "clock.h"

#define PIN_SERIAL1_TX   (10)  // PA10
#define PAD_SERIAL1_TX   (1)   // TXPO = 0
#define PIN_SERIAL1_RX   (11)  // PA11
#define PAD_SERIAL1_RX   (3)   // RXPO = 3


int main(void) {

  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);     // 1 ms tick

  printf("INIT\r\n");

  /* (Optional) UART for printf */
  uart_init(SERCOM2, 115200, PIN_SERIAL1_TX, PAD_SERIAL1_TX, PIN_SERIAL1_RX, PAD_SERIAL1_RX, 0);

  delay_ms(3000);
 
  uart_puts("\r\n", SERCOM2);
  char buf[64];
  size_t n = uart_read_blocking(buf, sizeof(buf), SERCOM2, 1000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   // semihosting console
  } else {
      printf("No loopback!\n");
  }
  uart_puts("radio get freq\r\n", SERCOM2);

  n = uart_read_blocking(buf, sizeof(buf), SERCOM2, 1000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   // semihosting console
  } else {
      printf("No loopback!\n");
  }

  uart_puts("radio set freq 868100000\r\n", SERCOM2);

  n = uart_read_blocking(buf, sizeof(buf), SERCOM2, 1000);
  if (n > 0) {
      printf("Loopback: %s\n", buf);   // semihosting console
  } else {
      printf("No loopback!\n");
  }
}