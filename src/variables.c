#include "variables.h"

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
	.sercom = SERCOM2,
	.tx_pin = 10,
	.tx_pad = 1,
	.rx_pin = 11,
	.rx_pad = 3,
	.baud = 115200,
	.pmux_func = 0,
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