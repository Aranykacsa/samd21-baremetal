// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"
#include "clock.h"

static void port_pinmux(uint8_t port, uint8_t pin, uint8_t function, int input_en)
{
    // PA10 = TX
PORT->Group[0].PINCFG[10].bit.PMUXEN = 1;
PORT->Group[0].PINCFG[10].bit.INEN   = 0;
PORT->Group[0].PMUX[10 >> 1].bit.PMUXE = PORT_PMUX_PMUXE_D_Val;

// PA11 = RX
PORT->Group[0].PINCFG[11].bit.PMUXEN = 1;
PORT->Group[0].PINCFG[11].bit.INEN   = 1;
PORT->Group[0].PMUX[11 >> 1].bit.PMUXO = PORT_PMUX_PMUXO_D_Val;

}


// src/drivers/uart/uart.c  (replace uart_init_115200_sercom5_pa10_pa11)
void uart_init(Sercom* sercom, uint32_t baud, uint8_t tx_pin, uint8_t txpo, uint8_t rx_pin, uint8_t rxpo, uint8_t pmux) {
  // APBC clock
  uint8_t gclk_id_core, gclk_id_slow;
  if (sercom == SERCOM0) {
      PM->APBCMASK.bit.SERCOM0_ = 1;
      gclk_id_core = SERCOM0_GCLK_ID_CORE;
      gclk_id_slow = SERCOM0_GCLK_ID_SLOW;
  } else if (sercom == SERCOM1) {
      PM->APBCMASK.bit.SERCOM1_ = 1;
      gclk_id_core = SERCOM1_GCLK_ID_CORE;
      gclk_id_slow = SERCOM1_GCLK_ID_SLOW;
  } else if (sercom == SERCOM2) {
      PM->APBCMASK.bit.SERCOM2_ = 1;
      gclk_id_core = SERCOM2_GCLK_ID_CORE;
      gclk_id_slow = SERCOM2_GCLK_ID_SLOW;
  } else if (sercom == SERCOM3) {
      PM->APBCMASK.bit.SERCOM3_ = 1;
      gclk_id_core = SERCOM3_GCLK_ID_CORE;
      gclk_id_slow = SERCOM3_GCLK_ID_SLOW;
  } else if (sercom == SERCOM4) {
      PM->APBCMASK.bit.SERCOM4_ = 1;
      gclk_id_core = SERCOM4_GCLK_ID_CORE;
      gclk_id_slow = SERCOM4_GCLK_ID_SLOW;
  } else if (sercom == SERCOM5) {
      PM->APBCMASK.bit.SERCOM5_ = 1;
      gclk_id_core = SERCOM5_GCLK_ID_CORE;
      gclk_id_slow = SERCOM5_GCLK_ID_SLOW;
  } 

  if (!pmux) {
    pmux = PORT_PMUX_PMUXE_D_Val;
  }

  // Feed SERCOM5 core + slow
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_core) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_slow) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  port_pinmux(0, tx_pin, pmux, 0);  // TX (output, no input buffer)
  port_pinmux(0, rx_pin, pmux, 1);  // RX (needs input buffer)

  // Reset
  sercom->USART.CTRLA.bit.SWRST = 1;
  while (sercom->USART.SYNCBUSY.bit.SWRST);

  // USART mode, internal clock, LSB first, 16x oversample
  // TX on PAD2 => TXPO=1, RX on PAD3 => RXPO=3
  sercom->USART.CTRLA.reg =
      SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
      SERCOM_USART_CTRLA_DORD |
      SERCOM_USART_CTRLA_SAMPR(0) |
      SERCOM_USART_CTRLA_TXPO(txpo) |
      SERCOM_USART_CTRLA_RXPO(rxpo);

  // Enable TX & RX, 8-bit, 1 stop bit (SBMODE=0 -> omit)
  sercom->USART.CTRLB.reg =
      SERCOM_USART_CTRLB_CHSIZE(0) |
      SERCOM_USART_CTRLB_TXEN |
      SERCOM_USART_CTRLB_RXEN;
  while (sercom->USART.SYNCBUSY.bit.CTRLB);

  // Baud = 65536 * (1 - 16*baud/Fref), Fref=48 MHz
  uint32_t ref = SystemCoreClock; // 1 MHz
  uint64_t br = (uint64_t)65536 * (ref - 16*baud) / ref;
  sercom->USART.BAUD.reg = (uint16_t)br;

  //sercom->USART.BAUD.reg = baud;

  sercom->USART.CTRLA.bit.ENABLE = 1;
  while (sercom->USART.SYNCBUSY.bit.ENABLE);
}

void uart_putc(char c, Sercom* sercom) {
  while (!sercom->USART.INTFLAG.bit.DRE);
  sercom->USART.DATA.reg = (uint16_t)c;
}

void uart_puts(const char *s, Sercom* sercom) {
  while (*s) {
    //if (*s == '\n') uart_putc('\r', sercom);
    printf("%c",*s);
    uart_putc(*s++, sercom);
  }
}

int uart_getc_blocking(Sercom* sercom) {
  while (!sercom->USART.INTFLAG.bit.RXC);
  return (int)(sercom->USART.DATA.reg & 0xFF);
}

int uart_try_read(Sercom* sercom) {
  if (!sercom->USART.INTFLAG.bit.RXC) return -1;
  return (int)(sercom->USART.DATA.reg & 0xFF);
}

size_t uart_read(char *buf, size_t maxlen, Sercom* sercom) {
  size_t n = 0;
  while (n < maxlen && sercom->USART.INTFLAG.bit.RXC) {
    buf[n++] = (sercom->USART.DATA.reg & 0xFF);
  }
  return n;
}


size_t uart_read_blocking(char *buf, size_t maxlen, Sercom* sercom, uint32_t timeout_ms) {
    size_t n = 0;
    uint32_t start = g_ms;
    while (n < maxlen - 1) {            // leave room for '\0'
        while (!sercom->USART.INTFLAG.bit.RXC) {
            if ((g_ms - start) >= timeout_ms) {
                buf[n] = '\0';          // terminate string
                return n;               // timeout reached
            }
        }
        buf[n++] = (sercom->USART.DATA.reg & 0xFF);
    }
    buf[n] = '\0';
    return n;
}
