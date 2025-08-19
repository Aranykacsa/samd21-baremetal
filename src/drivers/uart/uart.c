// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"

// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"

static inline void _sync5(void) { while (SERCOM5->USART.SYNCBUSY.reg) {} }

void uart_init_115200_sercom5_pa10_pa11(void) {
  // Clocks
  PM->APBCMASK.reg |= PM_APBCMASK_SERCOM5;
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(GCLK_CLKCTRL_ID_SERCOM5_CORE) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY) {}

  // Pinmux: PA10 (TX, PAD[2]) / PA11 (RX, PAD[3]) → function D
  PORT->Group[0].PINCFG[10].bit.PMUXEN = 1;
  PORT->Group[0].PINCFG[11].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[10/2].bit.PMUXE = PORT_PMUX_PMUXE_D_Val; // PA10 even
  PORT->Group[0].PMUX[11/2].bit.PMUXO = PORT_PMUX_PMUXO_D_Val; // PA11 odd

  // Reset
  SERCOM5->USART.CTRLA.bit.ENABLE = 0; _sync5();
  SERCOM5->USART.CTRLA.bit.SWRST  = 1;
  while (SERCOM5->USART.CTRLA.bit.SWRST || SERCOM5->USART.SYNCBUSY.bit.SWRST) {}

  // USART internal clock, LSB first, TX on PAD[2], RX on PAD[3], 16x oversampling
  SERCOM5->USART.CTRLA.reg =
      SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
      SERCOM_USART_CTRLA_DORD |           // LSB first (UART standard)
      SERCOM_USART_CTRLA_TXPO(1) |        // PAD[2] -> TX (PA10)
      SERCOM_USART_CTRLA_RXPO(3) |        // PAD[3] -> RX (PA11)
      SERCOM_USART_CTRLA_SAMPR(0);        // 16x

  // 8N1, enable TX/RX. (Do NOT set SBMODE -> stays 0 = 1 stop bit)
  SERCOM5->USART.CTRLB.reg =
      SERCOM_USART_CTRLB_CHSIZE(0) |      // 8-bit
      SERCOM_USART_CTRLB_TXEN |
      SERCOM_USART_CTRLB_RXEN;
  _sync5();

  // Baud: 48 MHz → 115200 (BAUD = 65536 * (1 - 16*baud/Fref)) ≈ 63019
  SERCOM5->USART.BAUD.reg = 63019;

  SERCOM5->USART.CTRLA.bit.ENABLE = 1;
  _sync5();
}

void uart_putc(uint8_t c) {
  while (!SERCOM5->USART.INTFLAG.bit.DRE) {}
  SERCOM5->USART.DATA.reg = c;
}


static inline void _gclk_enable(uint32_t id) {
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(id) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
}

void uart_puts(const char *s) {
  while (*s) {
    if (*s == '\n') uart_putc('\r');
    uart_putc(*s++);
  }
}

int uart_getc_blocking(void) {
  while (!SERCOM5->USART.INTFLAG.bit.RXC);
  return (int)(SERCOM5->USART.DATA.reg & 0xFF);
}

int uart_try_read(void) {
  if (!SERCOM5->USART.INTFLAG.bit.RXC) return -1;
  return (int)(SERCOM5->USART.DATA.reg & 0xFF);
}

size_t uart_read(uint8_t *buf, size_t maxlen) {
  size_t n = 0;
  while (n < maxlen && SERCOM5->USART.INTFLAG.bit.RXC) {
    buf[n++] = (uint8_t)(SERCOM5->USART.DATA.reg & 0xFF);
  }
  return n;
}
