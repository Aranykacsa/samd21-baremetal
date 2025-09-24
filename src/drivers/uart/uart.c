// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"

static void port_pinmux(uint8_t port, uint8_t pin, uint8_t function, int input_en) {
    PM->APBBMASK.bit.PORT_ = 1; // ensure PORT clock
    PORT->Group[port].PINCFG[pin].bit.PMUXEN = 1;
    PORT->Group[port].PINCFG[pin].bit.INEN   = input_en ? 1 : 0;

    if (pin & 1) {
        PORT->Group[port].PMUX[pin >> 1].bit.PMUXO = function;
    } else {
        PORT->Group[port].PMUX[pin >> 1].bit.PMUXE = function;
    }
}

void uart_init(uart_t* bus) {
  uint8_t gclk_id_core = 0, gclk_id_slow = 0;
  if (bus->sercom == SERCOM0) { PM->APBCMASK.bit.SERCOM0_ = 1; gclk_id_core = SERCOM0_GCLK_ID_CORE; gclk_id_slow = SERCOM0_GCLK_ID_SLOW; }
  else if (bus->sercom == SERCOM1) { PM->APBCMASK.bit.SERCOM1_ = 1; gclk_id_core = SERCOM1_GCLK_ID_CORE; gclk_id_slow = SERCOM1_GCLK_ID_SLOW; }
  else if (bus->sercom == SERCOM2) { PM->APBCMASK.bit.SERCOM2_ = 1; gclk_id_core = SERCOM2_GCLK_ID_CORE; gclk_id_slow = SERCOM2_GCLK_ID_SLOW; }
  else if (bus->sercom == SERCOM3) { PM->APBCMASK.bit.SERCOM3_ = 1; gclk_id_core = SERCOM3_GCLK_ID_CORE; gclk_id_slow = SERCOM3_GCLK_ID_SLOW; }
  else if (bus->sercom == SERCOM4) { PM->APBCMASK.bit.SERCOM4_ = 1; gclk_id_core = SERCOM4_GCLK_ID_CORE; gclk_id_slow = SERCOM4_GCLK_ID_SLOW; }
  else if (bus->sercom == SERCOM5) { PM->APBCMASK.bit.SERCOM5_ = 1; gclk_id_core = SERCOM5_GCLK_ID_CORE; gclk_id_slow = SERCOM5_GCLK_ID_SLOW; }

  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_core) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_slow) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // FIX: pass (port,pin), not (pin,pad)
  port_pinmux(bus->tx_port, bus->tx_pin, bus->pmux_func, 0);
  port_pinmux(bus->rx_port, bus->rx_pin, bus->pmux_func, 1);

  // Reset
  bus->sercom->USART.CTRLA.bit.SWRST = 1;
  while (bus->sercom->USART.SYNCBUSY.bit.SWRST);

  // USART config — NOTE: TXPO is a routing option (0->PAD0, 1->PAD2)
  bus->sercom->USART.CTRLA.reg =
      SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
      SERCOM_USART_CTRLA_DORD |
      SERCOM_USART_CTRLA_SAMPR(0) |
      SERCOM_USART_CTRLA_TXPO(bus->txpo) |  // e.g., 1 for TX on PAD2
      SERCOM_USART_CTRLA_RXPO(bus->rxpo);   // e.g., 3 for RX on PAD3

  bus->sercom->USART.CTRLB.reg =
      SERCOM_USART_CTRLB_CHSIZE(0) |
      SERCOM_USART_CTRLB_TXEN |
      SERCOM_USART_CTRLB_RXEN;
  while (bus->sercom->USART.SYNCBUSY.bit.CTRLB);

  // Safer BAUD calc
  uint64_t br = (uint64_t)65536 * (SystemCoreClock - 16ULL * bus->baud) / SystemCoreClock;
  bus->sercom->USART.BAUD.reg = (uint16_t)br;

  bus->sercom->USART.CTRLA.bit.ENABLE = 1;
  while (bus->sercom->USART.SYNCBUSY.bit.ENABLE);
}

void uart_write_char(uart_t* bus, char c) {
  while (!bus->sercom->USART.INTFLAG.bit.DRE);
  bus->sercom->USART.DATA.reg = (uint16_t)c;
}

void uart_write_string(uart_t* bus, const char *s) {
  uart_flush_rx(bus);
  while (*s) uart_write_char(bus, *s++);
}

int uart_read_char_blocking(uart_t* bus) {
  while (!bus->sercom->USART.INTFLAG.bit.RXC);
  return (int)(bus->sercom->USART.DATA.reg & 0xFF);
}

int uart_try_read(uart_t* bus) {
  if (!bus->sercom->USART.INTFLAG.bit.RXC) return -1;
  return (int)(bus->sercom->USART.DATA.reg & 0xFF);
}

size_t uart_read_string(uart_t* bus, char *buf, size_t len) {
  if (!len) return 0;
  size_t n = 0;
  while (n < (len - 1) && bus->sercom->USART.INTFLAG.bit.RXC) {
    buf[n++] = (char)(bus->sercom->USART.DATA.reg & 0xFF);
  }
  buf[n] = '\0'; // ensure termination
  return n;
}

void uart_flush_rx(uart_t* bus) {
    uint32_t guard = 4096;
    while (guard-- && bus->sercom->USART.INTFLAG.bit.RXC) {
        (void)(bus->sercom->USART.DATA.reg & 0xFF);
    }
}

size_t uart_readline(uart_t* bus, char *buf, size_t maxlen,
                     uint32_t overall_timeout_ms, uint32_t interchar_timeout_ms) {
    if (maxlen == 0) return 0;

    size_t n = 0;
    uint32_t t0  = get_uptime();
    uint32_t tch = t0; // inter-char timer

    while ((get_uptime() - t0) < overall_timeout_ms) {
        // wait for a char, but respect inter-char timeout
        while (!bus->sercom->USART.INTFLAG.bit.RXC) {
            uint32_t now = get_uptime();
            if ((now - t0)  >= overall_timeout_ms) goto out;
            if ((now - tch) >= interchar_timeout_ms) goto out;
        }

        char c = (char)(bus->sercom->USART.DATA.reg & 0xFF);
        tch = get_uptime(); // reset inter-char timer

        if (c == '\n' || c == '\r') break;    // end-of-line
        if (n + 1 < maxlen) buf[n++] = c;     // keep room for '\0'
    }

out:
    buf[n] = '\0';
    return n;
}


/*size_t uart_read_string_blocking(uart_t* bus, char *buf, size_t len, uint32_t timeout_ms) {
    if (!len) return 0;
    size_t n = 0;
    uint32_t start = get_uptime();
    while (n < (len - 1)) { // leave room for '\0'
        while (!bus->sercom->USART.INTFLAG.bit.RXC) {
            if ((get_uptime() - start) >= timeout_ms) {
                buf[n] = '\0';
                return n; // timeout
            }
        }
        buf[n++] = (char)(bus->sercom->USART.DATA.reg & 0xFF);
    }
    buf[n] = '\0';
    return n;
}*/
