// src/drivers/uart/uart.h
#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stddef.h>

void uart_init_115200_sercom5_pa10_pa11(void);
void uart_putc(uint8_t c);;
void uart_puts(const char *s);
int  uart_getc_blocking(void);     // blocks until a char arrives
int  uart_try_read(void);          // returns -1 if no char
size_t uart_read(uint8_t *buf, size_t maxlen); // non-blocking bulk read

#endif
