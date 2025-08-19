// src/drivers/uart/uart.h
#ifndef UART_H
#define UART_H

#include "samd21.h"
#include <stdint.h>
#include <stddef.h>

void uart_init(Sercom* sercom, uint32_t baud, uint8_t tx_pin, uint8_t txpo, uint8_t rx_pin, uint8_t rxpo, uint8_t pmux);
void uart_putc(char c, Sercom* sercom);
void uart_puts(const char *s, Sercom* sercom);
int  uart_getc_blocking(Sercom* sercom);     // blocks until a char arrives
int  uart_try_read(Sercom* sercom);          // returns -1 if no char
size_t uart_read(char *buf, size_t maxlen, Sercom* sercom); // non-blocking bulk read
size_t uart_read_blocking(char *buf, size_t maxlen, Sercom* sercom, uint32_t timeout_ms);

#endif