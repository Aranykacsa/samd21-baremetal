#include "board.h"
#include "variables.h"
#include "clock.h"
#include "uart.h"
#include "radio.h"
#include <stdint.h>

uint8_t setup_board(void) {
	uart_init(&uart_s2);

	delay_ms(3000);
}

uint8_t setup_peripherals(void) {
	setup_radio(true);
}