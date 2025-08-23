// src/drivers/uart/uart.h
#ifndef UART_H
#define UART_H

#include "samd21.h"
#include <stdint.h>
#include <stddef.h>
#include "variables.h"

void uart_init(uart_t* bus);
void uart_write_char(Sercom* sercom, char c);
void uart_write_string(Sercom* sercom, const char *s);
int  uart_read_char_blocking(Sercom* sercom);
int  uart_try_read(Sercom* sercom);     
size_t uart_read_string(Sercom* sercom, char *buf, size_t maxlen);
size_t uart_read_string_blocking(Sercom* sercom, char *buf, size_t maxlen, uint32_t timeout_ms);

#endif