#ifndef SPI_H
#define SPI_H

#include "variables.h"

void SD_CS_HIGH(spi_t* bus);
void SD_CS_LOW(spi_t* bus);

void spi_master_init(spi_t* bus);
void spi_set_baud(spi_t* bus, uint32_t hz);
void spi_enable_gclk(spi_t* bus);
void spi_pins_init(spi_t* bus);
void spi_chip_select_init(spi_t* bus);
uint8_t spi_txrx(spi_t* bus, uint8_t v);
void spi_wait_sync(spi_t* bus);

#endif /* SPI_H */