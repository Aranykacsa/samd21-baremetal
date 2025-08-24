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

extern spi_t spi_s1;

typedef struct {
	Sercom* sercom;
	uint8_t tx_pin, tx_pad;
	uint8_t rx_pin, rx_pad;
	uint32_t baud;
	uint8_t pmux_func;
} uart_t;

extern uart_t uart_s2;

typedef struct {
  Sercom*  sercom;
  uint8_t  sda_port, sda_pin;
  uint8_t  scl_port, scl_pin;
  uint8_t  pmux_func;       
  uint32_t hz;              
  uint32_t trise_ns;        
} i2c_t;

extern i2c_t i2c_s3;

#endif /* VARIABLES_H */