// src/drivers/uart/uart.h
#ifndef UART_H
#define UART_H

#include "samd21.h"
#include <stdint.h>
#include <stddef.h>
#include "variables.h"

// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"

uint16_t calculate_baud_value(uint32_t baudrate, uint32_t clock_hz);

// === Configure PA22/PA23 for SERCOM3 (MUX C) ===
void uart_pinmux(uart_t* bus);

// === Initialize SERCOM3 as UART (PA22 TX, PA23 RX) ===
void uart_init(uart_t* bus);

// === Transmit one byte ===
void uart_write(uart_t* bus, uint8_t data);

// === Receive one byte (blocking) ===
uint8_t uart_read(uart_t* bus);

// === Send a string ===
void uart_send_string(uart_t* bus, const char* s);

#endif
