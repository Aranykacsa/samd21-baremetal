// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"

static inline void _gclk_enable(uint32_t id) {
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(id) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
}

// src/drivers/uart/uart.c  (replace uart_init_115200_sercom5_pa10_pa11)
void uart_init_115200_sercom5_pa10_pa11(void) {
  // APBC clock
  PM->APBCMASK.bit.SERCOM5_ = 1;

  // Feed SERCOM5 core + slow
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM5_GCLK_ID_CORE) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM5_GCLK_ID_SLOW) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Pinmux: PA10 (TX, PAD2), PA11 (RX, PAD3) -> function D
  PORT->Group[0].PINCFG[10].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[10/2].bit.PMUXE  = PORT_PMUX_PMUXE_D_Val;
  PORT->Group[0].PINCFG[11].bit.PMUXEN = 1;
  PORT->Group[0].PINCFG[11].bit.INEN   = 1;
  PORT->Group[0].PMUX[11/2].bit.PMUXO  = PORT_PMUX_PMUXO_D_Val;

  // Reset
  SERCOM5->USART.CTRLA.bit.SWRST = 1;
  while (SERCOM5->USART.SYNCBUSY.bit.SWRST);

  // USART mode, internal clock, LSB first, 16x oversample
  // TX on PAD2 => TXPO=1, RX on PAD3 => RXPO=3
  SERCOM5->USART.CTRLA.reg =
      SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
      SERCOM_USART_CTRLA_DORD |
      SERCOM_USART_CTRLA_SAMPR(0) |
      SERCOM_USART_CTRLA_TXPO(1) |
      SERCOM_USART_CTRLA_RXPO(3);

  // Enable TX & RX, 8-bit, 1 stop bit (SBMODE=0 -> omit)
  SERCOM5->USART.CTRLB.reg =
      SERCOM_USART_CTRLB_CHSIZE(0) |
      SERCOM_USART_CTRLB_TXEN |
      SERCOM_USART_CTRLB_RXEN;
  while (SERCOM5->USART.SYNCBUSY.bit.CTRLB);

  // Baud = 65536 * (1 - 16*baud/Fref), Fref=48 MHz
  const uint32_t baud = 115200;
  const uint64_t br = (uint64_t)65536 * (48000000 - 16*baud) / 48000000;
  SERCOM5->USART.BAUD.reg = (uint16_t)br;

  SERCOM5->USART.CTRLA.bit.ENABLE = 1;
  while (SERCOM5->USART.SYNCBUSY.bit.ENABLE);
}


void uart_putc(char c) {
  while (!SERCOM5->USART.INTFLAG.bit.DRE);
  SERCOM5->USART.DATA.reg = (uint16_t)c;
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
