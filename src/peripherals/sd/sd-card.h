#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>
#include "variables.h"

int sd_init(spi_t* bus);
int sd_read_block(spi_t* bus, uint32_t lba, uint8_t *dst512);
int sd_write_block(spi_t* bus, uint32_t lba, const uint8_t *src512);
int sd_is_sdhc(spi_t bus*);
void sd_spi_set_hz(spi_t* bus, uint32_t hz);

#endif /* SD_CARD_H */