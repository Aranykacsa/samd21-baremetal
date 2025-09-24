#ifndef SD_HELPER_H
#define SD_HELPER_H

#include <stdint.h>
#include "variables.h"

uint8_t sd_init(spi_t* bus);
uint8_t sd_read_block(spi_t* bus, uint32_t lba, uint8_t *dst512);
uint8_t sd_write_block(spi_t* bus, uint32_t lba, const uint8_t *src512);
uint8_t sd_is_sdhc(void);
uint8_t sd_spi_set_hz(spi_t* bus, uint32_t hz);

#endif /* SD_HELPER_H */