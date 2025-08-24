#ifndef VARIABLES_H
#define VARIABLES_H

#include "samd21.h"
#include "system_samd21.h"

typedef struct {
	Sercom* sercom;
	uint8_t mosi_pin, mosi_port;
	uint8_t miso_pin, miso_port;
	uint8_t sck_pin, sck_port;
	uint8_t cs_pin, cs_port;
	uint32_t init_hz, run_hz;
	uint32_t cmd_timeout, token_timeout;
} spi_t;

static spi_t spi_s1 = {
	.sercom = SERCOM1,
	.mosi_pin = 16, .mosi_port = 0,
	.miso_pin = 19, .miso_port = 0,
	.sck_pin = 17, .sck_port = 0,
	.cs_pin = 18, .cs_port = 0,
	.init_hz = 400000u, .run_hz = 8000000u,
	.cmd_timeout = 100u, .token_timeout = 100u,
};

typedef struct {
	Sercom* sercom;
	uint8_t tx_pin, tx_pad;
	uint8_t rx_pin, rx_pad;
	uint32_t baud;
	uint8_t pmux_func;
} uart_t;

static uart_t uart_s2 = {
	.sercom = SERCOM2,
	.tx_pin = 10,
	.tx_pad = 1,
	.rx_pin = 11,
	.rx_pad = 3,
	.baud = 115200,
	.pmux_func = 0,
};

typedef struct {
  Sercom*  sercom;
  uint8_t  sda_port, sda_pin;
  uint8_t  scl_port, scl_pin;
  uint8_t  pmux_func;       
  uint32_t hz;              
  uint32_t trise_ns;        
} i2c_t;

static i2c_t i2c_s3 = {
	.sercom = SERCOM3,
	.hz = 400000, 
	.sda_port = 0,
	.sda_pin = 22, 
	.scl_port = 0,
	.scl_pin = 23, 
	.pmux_func = 2, 
	.trise_ns = 120,
};

#endif /* VARIABLES_H */