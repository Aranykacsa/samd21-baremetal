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
size_t uart_read_string(uart_t* bus, char *buf);
size_t uart_read_string_blocking(uart_t* bus, char *buf, uint32_t timeout_ms);

#endif