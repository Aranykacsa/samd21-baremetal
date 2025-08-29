#include "variables.h"
#include <stddef.h>

spi_t spi_s1 = {
	.sercom = SERCOM1,
	.mosi_pin = 16, .mosi_port = 0,
	.miso_pin = 19, .miso_port = 0,
	.sck_pin = 17, .sck_port = 0,
	.cs_pin = 18, .cs_port = 0,
	.init_hz = 400000u, .run_hz = 8000000u,
	.cmd_timeout = 100u, .token_timeout = 100u,
};

uart_t uart_s2 = {
  .sercom    = SERCOM2,
  .tx_port   = 0,     // PORTA
  .tx_pin    = 10,    // PA10 -> SERCOM2/PAD2
  .rx_port   = 0,     // PORTA
  .rx_pin    = 11,    // PA11 -> SERCOM2/PAD3
  .txpo      = 1,     // TX on PAD2
  .rxpo      = 3,     // RX on PAD3
  .baud      = 115200,
  .pmux_func = 3      // Function D (SERCOM ALT)
};

i2c_t i2c_s3 = {
	.sercom = SERCOM3,
	.hz = 400000, 
	.sda_port = 0,
	.sda_pin = 22, 
	.scl_port = 0,
	.scl_pin = 23, 
	.pmux_func = 2, 
	.trise_ns = 120,
};

const char* radio_commands[] = {
    "mod lora",
    "freq 868100000",
    "sf sf12",
    "pa on",
    "pwr 20"
};

const size_t radio_commands_len =
    sizeof(radio_commands) / sizeof(radio_commands[0]);
