// src/drivers/uart/uart.h
#ifndef UART_H
#define UART_H

#include "samd21.h"
#include <stdint.h>
#include <stddef.h>
#include "variables.h"

void uart_init(uart_t* bus);
void uart_write_char(uart_t* bus, char c);
void uart_write_string(uart_t* bus, const char *s);
int  uart_read_char_blocking(uart_t* bus);
int  uart_try_read(uart_t* bus);     
size_t uart_read_string(uart_t* bus, char *buf, size_t len);
//size_t uart_read_string_blocking(uart_t* bus, char *buf, size_t len, uint32_t timeout_ms);

// NEW: flush pending bytes from RX FIFO
void uart_flush_rx(uart_t* bus);

// NEW: read one CR/LF terminated line with timeouts
size_t uart_readline(uart_t* bus, char *buf, size_t maxlen,
                     uint32_t overall_timeout_ms, uint32_t interchar_timeout_ms);

// (optional) keep this, but we won’t use it anymore:
// size_t uart_read_string_blocking(uart_t* bus, char *buf, size_t len, uint32_t timeout_ms);

static inline void uart_drain_tx(uart_t* bus) {
    while (!bus->sercom->USART.INTFLAG.bit.TXC) {}
}


#endif