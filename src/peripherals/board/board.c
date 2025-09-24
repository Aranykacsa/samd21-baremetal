#include "board.h"
#include "variables.h"

// Interfaces
#include "clock.h"
#include "uart.h"
#include "spi.h"
#include "i2c-master.h"

// Peripherals
#include "radio.h"
#include "storage.h"
#include "aht20.h"

// Addons
#include <stdint.h>

uint8_t setup_board(void) {
	uart_init(&uart_s2);
	spi_master_init(&spi_s1);
    i2c_m_init(&i2c_s3);
	delay_ms(3000);
	return 0;
}

uint8_t setup_peripherals(void) {
	uint8_t status;
	
	status = setup_radio(true);
	if(status != 0) return status;
	status = setup_storage();
	if(status != 0) return status;
	status = aht20_init(&i2c_s3, AHT_ADDR);
	if(status != 0) return status;
	
	return 0;
}

uint8_t measure_env(float* temp, float* hum) {
    if (aht20_read(&i2c_s3, AHT_ADDR, temp, hum) == 0) {
	    printf("T=%.2f C  RH=%.2f %%\r\n", (double)*temp, (double)*hum);    
	}

	return 0;
}