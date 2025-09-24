#include "storage.h"
#include "variables.h"
#include "sd-helper.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>

/* ---- Return codes ---- */
#define STORAGE_OK            10
#define STORAGE_ERR_SD        11
#define STORAGE_ERR_PARAM     12
#define STORAGE_ERR_FULL      13
#define STORAGE_ERR_LOG_FULL  14

#define SECTOR_SIZE 512
#define CRC_SIZE 4
#define HEADER_SIZE 1
#define PAYLOAD_SIZE (SECTOR_SIZE - CRC_SIZE - HEADER_SIZE)

#define RAID_MIRRORS 3


/*### HELPERS ###*/
/**
 * @brief Compute a 32-bit CRC (Cyclic Redundancy Check) using the standard
 *        Ethernet/ZIP polynomial (0xEDB88320).
 *
 * This implementation follows the "bitwise" algorithm:
 *  - The CRC register is initialized to 0xFFFFFFFF.
 *  - Each byte of input data is XORed into the CRC.
 *  - For each of the 8 bits in the byte, the least significant bit of the CRC
 *    determines whether to shift right only or to also XOR with the polynomial.
 *  - After all input bytes are processed, the CRC is bitwise inverted
 *    (XOR with 0xFFFFFFFF) to produce the final value.
 *
 * The resulting CRC is equivalent to the one produced by zlib's crc32(),
 * POSIX `cksum -o3`, and Ethernet CRC32. This guarantees cross-platform
 * compatibility (firmware vs. host tools).
 *
 * @note This function is intentionally implemented bit-by-bit for clarity and
 *       portability. For high-throughput logging, a lookup-table version may
 *       be substituted.
 *
 * @param data Pointer to the input buffer to compute CRC over.
 * @param len  Number of bytes in the input buffer.
 *
 * @return 32-bit CRC value (little-endian stored in sector).
 *
 * @par Usage in the logging filesystem
 * Each SD card sector (512 bytes) is structured as:
 * - Byte 0: Header (e.g. fixed marker = 1)
 * - Bytes 1..507: Payload (507 bytes of user/application data)
 * - Bytes 508..511: CRC32 checksum (little-endian)
 *
 * When writing a sector:
 * - CRC is computed over header + payload (bytes 0..507).
 * - Resulting 32-bit CRC is split into 4 bytes and stored at positions 508..511.
 *
 * When reading a sector:
 * - The CRC is re-computed over bytes 0..507.
 * - It must match the 4-byte stored CRC at 508..511, otherwise the sector is invalid.
 *
 * Example:
 * @code
 * uint32_t crc = crc32_u8bit(sector_buffer, 508);
 * sector_buffer[508] = (uint8_t)(crc & 0xFF);
 * sector_buffer[509] = (uint8_t)((crc >> 8) & 0xFF);
 * sector_buffer[510] = (uint8_t)((crc >> 16) & 0xFF);
 * sector_buffer[511] = (uint8_t)((crc >> 24) & 0xFF);
 * @endcode
 *
 * This guarantees that host-side verification tools (bash/C decoder)
 * and on-device firmware both produce the same CRC values.
 */
uint32_t crc32_u8bit(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

uint8_t get_last_sector(uint32_t* last_sector) {
  if (!last_sector) return STORAGE_ERR_PARAM;

  uint8_t buffer[512];
  uint8_t rc = sd_read_block(&spi_s1, log_sector, buffer);
  if (rc != 0x00) return STORAGE_ERR_SD;

  *last_sector =
      ((uint32_t)buffer[0]) |
      ((uint32_t)buffer[1] << 8) |
      ((uint32_t)buffer[2] << 16);

  return STORAGE_OK;
}

uint8_t set_last_sector(const uint32_t* last_sector) {
  if (!last_sector) return STORAGE_ERR_PARAM;

  uint8_t buffer[512];
  uint8_t rc = sd_read_block(&spi_s1, log_sector, buffer);
  if (rc != 0x00) return STORAGE_ERR_SD;

  buffer[0] = (uint8_t)(*last_sector & 0xFF);
  buffer[1] = (uint8_t)((*last_sector >> 8) & 0xFF);
  buffer[2] = (uint8_t)((*last_sector >> 16) & 0xFF);

  rc = sd_write_block(&spi_s1, log_sector, buffer);
  return (rc == 0x00) ? STORAGE_OK : STORAGE_ERR_SD;
}

/*### PUBLIC API ###*/
uint8_t setup_storage(void) {
  uint8_t rc = sd_init(&spi_s1);
  printf("[STORAGE] sd_init: %d\r\n", rc);
  return rc;  
}

uint8_t test_save_msg(void) {
  uint8_t err;
  uint8_t msg = 5;
  for(uint32_t i = 0; i < 1024; i++) {
    err = save_msg(&msg);
    if(err != STORAGE_OK) return err;
  }

  return STORAGE_OK;
}

uint8_t save_msg(uint8_t* msg) {
  uint8_t buffer[512];
  uint16_t last_msg;
  uint8_t is_first_full;

  uint8_t rc = sd_read_block(&spi_s1, log_sector, buffer);
  if (rc != 0x00) return STORAGE_ERR_SD;

  is_first_full = buffer[5];

  last_msg =
      ((uint16_t)buffer[3]) |
      ((uint16_t)buffer[4] << 8);
    
  last_msg++;

  if(is_first_full) {
    if(last_msg == 512) {
      return STORAGE_ERR_LOG_FULL;
    } else {
      buffer[3] = (uint8_t)(last_msg & 0xFF);
      buffer[4] = (uint8_t)((last_msg >> 8) & 0xFF);

      rc = sd_write_block(&spi_s1, log_sector, buffer);
      if (rc != 0x00) return STORAGE_ERR_SD;

      rc = sd_read_block(&spi_s1, log_sector + 1, buffer);
      if (rc != 0x00) return STORAGE_ERR_SD;

      buffer[last_msg] = *msg;

      rc = sd_write_block(&spi_s1, log_sector + 1, buffer);
      if (rc != 0x00) return STORAGE_ERR_SD;
    }
  } else {
    if(last_msg == 512) {
      last_msg = 0;
      is_first_full = 1;

      buffer[3] = (uint8_t)(last_msg & 0xFF);
      buffer[4] = (uint8_t)((last_msg >> 8) & 0xFF);
      buffer[5] = is_first_full;

      rc = sd_write_block(&spi_s1, log_sector, buffer);
      if (rc != 0x00) return STORAGE_ERR_SD;

      rc = sd_read_block(&spi_s1, log_sector + 1, buffer);
      if (rc != 0x00) return STORAGE_ERR_SD;

      buffer[last_msg] = *msg;

      rc = sd_write_block(&spi_s1, log_sector + 1, buffer);
      if (rc != 0x00) return STORAGE_ERR_SD;

    } else {
      buffer[3] = (uint8_t)(last_msg & 0xFF);
      buffer[4] = (uint8_t)((last_msg >> 8) & 0xFF);
      buffer[last_msg] = *msg;

      rc = sd_write_block(&spi_s1, log_sector, buffer);
      if (rc != 0x00) return STORAGE_ERR_SD;
    }
  }

  return STORAGE_OK;
}

uint8_t raid_u8bit_values(uint8_t* buffer, size_t len) {
  uint8_t rc;

  for(uint8_t i = 0; i < RAID_MIRRORS; i++) {
    rc = save_u8bit_values(buffer, len);
    if(rc != STORAGE_OK) return rc;
  }

  return STORAGE_OK;
}

uint8_t save_u8bit_values(uint8_t* buffer, size_t len) {
    if (!buffer) return STORAGE_ERR_PARAM;
    if (len % PAYLOAD_SIZE != 0) return STORAGE_ERR_PARAM;

    uint32_t last_sector = 0;
    uint8_t rc = get_last_sector(&last_sector);
    if (rc != STORAGE_OK) return rc;

    uint16_t num_of_sectors = len / PAYLOAD_SIZE;
    uint32_t new_sector = last_sector;

    uint8_t sector_buffer[SECTOR_SIZE];

    for (uint16_t i = 0; i < num_of_sectors; i++) {
        new_sector++;

        // clear buffer to avoid garbage
        for (uint16_t j = 0; j < SECTOR_SIZE; j++) sector_buffer[j] = 0;

        // header
        sector_buffer[0] = 1;

        // payload
        for (uint16_t k = 0; k < PAYLOAD_SIZE; k++) {
            sector_buffer[1 + k] = buffer[i * PAYLOAD_SIZE + k];
        }

        // CRC over header + payload (508 bytes)
        uint32_t crc = crc32_u8bit(sector_buffer, HEADER_SIZE + PAYLOAD_SIZE);

        // store CRC
        sector_buffer[508] = (uint8_t)(crc & 0xFF);
        sector_buffer[509] = (uint8_t)((crc >> 8) & 0xFF);
        sector_buffer[510] = (uint8_t)((crc >> 16) & 0xFF);
        sector_buffer[511] = (uint8_t)((crc >> 24) & 0xFF);

        rc = sd_write_block(&spi_s1, new_sector, sector_buffer);
        if (rc != 0x00) return STORAGE_ERR_SD;
    }

    rc = set_last_sector(&new_sector);
    return (rc == STORAGE_OK) ? STORAGE_OK : rc;
}


/* First 3bytes => last sector; 4th and 5th bytes => last log; 6th byte => first log full == 1 */
uint8_t init_log_sector(void) {
    uint8_t buffer[SECTOR_SIZE];
    for (uint16_t i = 0; i < SECTOR_SIZE; i++) buffer[i] = 0;

    // clear the "msg buffer" sector too
    uint8_t rc = sd_write_block(&spi_s1, log_sector + 1, buffer);
    if (rc != 0x00) return STORAGE_ERR_SD;

    // reset buffer again
    for (uint16_t i = 0; i < SECTOR_SIZE; i++) buffer[i] = 0;

    const uint32_t start_sector = 1;
    const uint16_t last_msg = 0;   // start empty (was 10 before)

    buffer[0] = (uint8_t)(start_sector & 0xFF);
    buffer[1] = (uint8_t)((start_sector >> 8) & 0xFF);
    buffer[2] = (uint8_t)((start_sector >> 16) & 0xFF);

    buffer[3] = (uint8_t)(last_msg & 0xFF);
    buffer[4] = (uint8_t)((last_msg >> 8) & 0xFF);

    rc = sd_write_block(&spi_s1, log_sector, buffer);
    return (rc == 0x00) ? STORAGE_OK : STORAGE_ERR_SD;
}

