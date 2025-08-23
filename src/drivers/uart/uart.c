// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"

static void port_pinmux(uint8_t port, uint8_t pin, uint8_t function, int input_en) {
    // Enable PMUX on this pin
    PORT->Group[port].PINCFG[pin].bit.PMUXEN = 1;
    PORT->Group[port].PINCFG[pin].bit.INEN   = input_en ? 1 : 0;

    // Each PMUX register controls two pins: even = PMUXE, odd = PMUXO
    if (pin & 1) {
        PORT->Group[port].PMUX[pin >> 1].bit.PMUXO = function;
    } else {
        PORT->Group[port].PMUX[pin >> 1].bit.PMUXE = function;
    }
}

void uart_init(uart_t* bus) {
  uint8_t gclk_id_core = 0, gclk_id_slow = 0;
  if (bus->sercom == SERCOM0) {
      PM->APBCMASK.bit.SERCOM0_ = 1;
      gclk_id_core = SERCOM0_GCLK_ID_CORE;
      gclk_id_slow = SERCOM0_GCLK_ID_SLOW;
  } else if (bus->sercom == SERCOM1) {
      PM->APBCMASK.bit.SERCOM1_ = 1;
      gclk_id_core = SERCOM1_GCLK_ID_CORE;
      gclk_id_slow = SERCOM1_GCLK_ID_SLOW;
  } else if (bus->sercom == SERCOM2) {
      PM->APBCMASK.bit.SERCOM2_ = 1;
      gclk_id_core = SERCOM2_GCLK_ID_CORE;
      gclk_id_slow = SERCOM2_GCLK_ID_SLOW;
  } else if (bus->sercom == SERCOM3) {
      PM->APBCMASK.bit.SERCOM3_ = 1;
      gclk_id_core = SERCOM3_GCLK_ID_CORE;
      gclk_id_slow = SERCOM3_GCLK_ID_SLOW;
  } else if (bus->sercom == SERCOM4) {
      PM->APBCMASK.bit.SERCOM4_ = 1;
      gclk_id_core = SERCOM4_GCLK_ID_CORE;
      gclk_id_slow = SERCOM4_GCLK_ID_SLOW;
  } else if (bus->sercom == SERCOM5) {
      PM->APBCMASK.bit.SERCOM5_ = 1;
      gclk_id_core = SERCOM5_GCLK_ID_CORE;
      gclk_id_slow = SERCOM5_GCLK_ID_SLOW;
  } 

  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_core) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_slow) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  port_pinmux(bus->tx_pad, bus->tx_pin, bus->pmux_func, 0);
  port_pinmux(bus->rx_pad, bus->rx_pin, bus->pmux_func, 1);

  // Reset
  bus->sercom->USART.CTRLA.bit.SWRST = 1;
  while (bus->sercom->USART.SYNCBUSY.bit.SWRST);

  // USART mode, internal clock, LSB first, 16x oversample
  // TX on PAD2 => TXPO=1, RX on PAD3 => RXPO=3
  bus->sercom->USART.CTRLA.reg =
      SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
      SERCOM_USART_CTRLA_DORD |
      SERCOM_USART_CTRLA_SAMPR(0) |
      SERCOM_USART_CTRLA_TXPO(bus->tx_pad) |
      SERCOM_USART_CTRLA_RXPO(bus->rx_pad);

  // Enable TX & RX, 8-bit, 1 stop bit (SBMODE=0 -> omit)
  bus->sercom->USART.CTRLB.reg =
      SERCOM_USART_CTRLB_CHSIZE(0) |
      SERCOM_USART_CTRLB_TXEN |
      SERCOM_USART_CTRLB_RXEN;
  while (bus->sercom->USART.SYNCBUSY.bit.CTRLB);

  uint64_t br = (uint64_t)65536 * (SystemCoreClock - 16*bus->baud) / SystemCoreClock;
  bus->sercom->USART.BAUD.reg = (uint16_t)br;


  bus->sercom->USART.CTRLA.bit.ENABLE = 1;
  while (bus->sercom->USART.SYNCBUSY.bit.ENABLE);
}

void uart_write_char(Sercom* sercom, char c) {
  while (!sercom->USART.INTFLAG.bit.DRE);
  sercom->USART.DATA.reg = (uint16_t)c;
}

void uart_write_string(Sercom* sercom, const char *s) {
  while (*s) {
    uart_write_char(sercom, *s++);
  }
}

int uart_read_char_blocking(Sercom* sercom) {
  while (!sercom->USART.INTFLAG.bit.RXC);
  return (int)(sercom->USART.DATA.reg & 0xFF);
}

int uart_try_read(Sercom* sercom) {
  if (!sercom->USART.INTFLAG.bit.RXC) return -1;
  return (int)(sercom->USART.DATA.reg & 0xFF);
}

size_t uart_read_string(Sercom* sercom, char *buf, size_t maxlen) {
  size_t n = 0;
  while (n < maxlen && sercom->USART.INTFLAG.bit.RXC) {
    buf[n++] = (sercom->USART.DATA.reg & 0xFF);
  }
  return n;
}


size_t uart_read_string_blocking(Sercom* sercom, char *buf, size_t maxlen, uint32_t timeout_ms) {
    size_t n = 0;
    uint32_t start = get_uptime();
    while (n < maxlen - 1) {            // leave room for '\0'
        while (!sercom->USART.INTFLAG.bit.RXC) {
          uint32_t now = get_uptime();
            if ((now - start) >= timeout_ms) {
                buf[n] = '\0';          // terminate string
                return n;               // timeout reached
            }
        }
        buf[n++] = (sercom->USART.DATA.reg & 0xFF);
    }
    buf[n] = '\0';
    return n;
}
