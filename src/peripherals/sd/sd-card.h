#ifndef SD_CARD_H
#define SD_CARD_H

#include <stdint.h>

/* Initialize SERCOM1 SPI on PA16/PA17/PA19 and CS on PA18; bring up SD card.
   Returns 0 on success, nonzero on failure. */
int sd_init(void);

/* Read/write one 512-byte block. LBA is block index (not byte addr).
   Returns 0 on success. */
int sd_read_block(uint32_t lba, uint8_t *dst512);
int sd_write_block(uint32_t lba, const uint8_t *src512);

/* Optional: get if card is high capacity (SDHC/SDXC) */
int sd_is_sdhc(void);

/* Low-level, if you want to retune SPI after init */
void sd_spi_set_hz(uint32_t hz);

#endif /* SD_CARD_H */