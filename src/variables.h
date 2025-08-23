#ifndef VARIABLES_H
#define VARIABLES_H

#include "samd21.h"
#include "system_samd21.h"

#define PIN_SERIAL1_TX   (10) 
#define PAD_SERIAL1_TX   (1)   
#define PIN_SERIAL1_RX   (11)  
#define PAD_SERIAL1_RX   (3)   

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