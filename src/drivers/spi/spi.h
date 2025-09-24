#ifndef SPI_H
#define SPI_H

#include <stdint.h>
#include "samd21.h"
#include "variables.h"

// Your spi_t is presumably declared elsewhere (variables.h or spi.h).
// If it's here, keep it; if elsewhere, remove this comment.

/* Return codes */
#define SPI_OK        0x00
#define SPI_ERR_SYNC  0x01
#define SPI_ERR_PARAM 0x02

/* Chip-select helpers now return status */
uint8_t SD_CS_HIGH(spi_t* bus);
uint8_t SD_CS_LOW(spi_t* bus);

/* Wait for SYNCBUSY clear — returns status */
uint8_t spi_wait_sync(spi_t* bus);

/* Full-duplex byte TX/RX with explicit output pointer */
uint8_t spi_txrx(spi_t* bus, uint8_t v, uint8_t *out);

/* Init helpers now return status */
uint8_t spi_chip_select_init(spi_t* bus);
uint8_t spi_pins_init(spi_t* bus);
uint8_t spi_enable_gclk(spi_t* bus);

/* Baud setter returns status */
uint8_t spi_set_baud(spi_t* bus, uint32_t hz);

/* Master init returns status */
uint8_t spi_master_init(spi_t* bus);

#endif /* SPI_H */
