#include "sd-helper.h"
#include "samd21.h"
#include <stdint.h>
#include <stddef.h>
#include "clock.h"
#include <stdio.h>
#include "variables.h"
#include "spi.h"

/* ==== Return codes ==== */
#define SD_OK                 0x00
#define SD_ERR_SPI            0x01
#define SD_ERR_TIMEOUT        0x02
#define SD_ERR_BAD_R1         0x03
#define SD_ERR_PARAM          0x04
#define SD_ERR_INIT           0x05
#define SD_ERR_TOKEN          0x06
#define SD_ERR_RESP           0x07

/* Card type flag */
static uint8_t g_is_sdhc = 0;

/* Public setter if you want to bump speed after init */
uint8_t sd_spi_set_hz(spi_t* bus, uint32_t hz) {
  return spi_set_baud(bus, hz); // returns 0x00 on success
}

/* ==== SPI byte helpers with status ==== */

static uint8_t sd_spi_recv(spi_t* bus, uint8_t *byte) {
  uint8_t rc = spi_txrx(bus, 0xFF, byte);
  return (rc == 0x00) ? SD_OK : SD_ERR_SPI;
}

static uint8_t sd_spi_send(spi_t* bus, uint8_t v) {
  uint8_t dummy;
  uint8_t rc = spi_txrx(bus, v, &dummy);
  return (rc == 0x00) ? SD_OK : SD_ERR_SPI;
}

static uint8_t sd_spi_send_bytes(spi_t* bus, const uint8_t *p, uint32_t n){
  while (n--) {
    uint8_t rc = sd_spi_send(bus, *p++);
    if (rc) return rc;
  }
  return SD_OK;
}

static uint8_t sd_spi_recv_bytes(spi_t* bus, uint8_t *p, uint32_t n){
  while (n--) {
    uint8_t rc = sd_spi_recv(bus, p++);
    if (rc) return rc;
  }
  return SD_OK;
}

/* >= 74 clocks with CS high */
static uint8_t sd_clock_idle(spi_t* bus, uint32_t clocks){
  uint8_t rc = SD_CS_HIGH(bus); if (rc) return rc;
  for (uint32_t i = 0; i < clocks/8u; i++) {
    rc = sd_spi_send(bus, 0xFF);
    if (rc) return rc;
  }
  return SD_OK;
}

static uint8_t sd_wait_r1(spi_t* bus, uint8_t *r1_out, uint32_t ms){
  uint8_t v = 0xFF;
  while (ms--) {
    uint8_t rc = sd_spi_recv(bus, &v);
    if (rc) return rc;
    if ((v & 0x80) == 0) { *r1_out = v; return SD_OK; }
    delay_ms(1);
  }
  return SD_ERR_TIMEOUT;
}

static uint8_t sd_wait_token(spi_t* bus, uint8_t token, uint32_t ms){
  uint8_t b = 0xFF;
  while (ms--) {
    uint8_t rc = sd_spi_recv(bus, &b);
    if (rc) return rc;
    if (b == token) return SD_OK;
    delay_ms(1);
  }
  return SD_ERR_TOKEN;
}

/* Send command: cmd=0..63 (no 0x40), arg big-endian, return R1 in *r1_out */
static uint8_t sd_cmd_r1(spi_t* bus, uint8_t cmd, uint32_t arg, uint8_t crc, uint8_t *r1_out){
  uint8_t rc;
  uint8_t frame[6];
  frame[0] = 0x40 | (cmd & 0x3F);
  frame[1] = (uint8_t)(arg >> 24);
  frame[2] = (uint8_t)(arg >> 16);
  frame[3] = (uint8_t)(arg >> 8);
  frame[4] = (uint8_t)(arg);
  frame[5] = crc;

  rc = SD_CS_LOW(bus);  if (rc) return rc;
  rc = sd_spi_send(bus, 0xFF); if (rc) { SD_CS_HIGH(bus); return rc; } // stuff byte

  rc = sd_spi_send_bytes(bus, frame, 6);
  if (rc) { SD_CS_HIGH(bus); return rc; }

  rc = sd_wait_r1(bus, r1_out, spi_s1.cmd_timeout);
  return rc; // caller will release CS
}

static uint8_t sd_cs_release(spi_t* bus){
  uint8_t rc = SD_CS_HIGH(bus);
  if (rc) return rc;
  // one extra clock
  return sd_spi_send(bus, 0xFF);
}

/* ==== SD public API ==== */

uint8_t sd_is_sdhc(void){ return g_is_sdhc; }

static uint8_t sd_go_idle(spi_t* bus){
  // CMD0 with CRC 0x95, expect R1=0x01 (idle)
  for (int i = 0; i < 10; i++){
    uint8_t r1 = 0xFF;
    uint8_t rc = sd_cmd_r1(bus, 0, 0, 0x95, &r1);
    uint8_t rc2 = sd_cs_release(bus);
    if (rc) return rc;
    if (rc2) return rc2;

    if (r1 == 0x01) return SD_OK;
    delay_ms(10);
  }
  return SD_ERR_TIMEOUT;
}

static uint8_t sd_check_if_v2_and_voltage_ok(spi_t* bus, uint32_t *ocr_out){
  // CMD8 VHS=0x1, pattern 0xAA, CRC 0x87
  uint8_t r1 = 0xFF, rc;
  rc = sd_cmd_r1(bus, 8, 0x000001AAu, 0x87, &r1);
  if (rc) { sd_cs_release(bus); return rc; }

  if (r1 & 0x04) { sd_cs_release(bus); return SD_OK; } // illegal cmd => v1.x (not fatal)
  if (r1 != 0x01){ sd_cs_release(bus); return SD_ERR_BAD_R1; }

  uint8_t r7[4];
  rc = sd_spi_recv_bytes(bus, r7, 4);
  uint8_t rc2 = sd_cs_release(bus);
  if (rc) return rc;
  if (rc2) return rc2;

  if (r7[3] != 0xAA) return SD_ERR_RESP;
  if (ocr_out) *ocr_out = ((uint32_t)r7[0] << 24) | ((uint32_t)r7[1] << 16) | ((uint32_t)r7[2] << 8) | r7[3];
  return SD_OK;
}

static uint8_t sd_send_acmd41_hcs(spi_t* bus){
  for (uint32_t ms = 0; ms < 1000; ms += 20){
    uint8_t r1 = 0xFF, rc;

    // APP_CMD (CMD55)
    rc = sd_cmd_r1(bus, 55, 0, 0xFF, &r1);
    uint8_t rc2 = sd_cs_release(bus);
    if (rc) return rc;
    if (rc2) return rc2;
    if (r1 > 0x01) return SD_ERR_BAD_R1;

    // ACMD41 with HCS
    rc = sd_cmd_r1(bus, 41, 0x40000000u, 0xFF, &r1);
    rc2 = sd_cs_release(bus);
    if (rc) return rc;
    if (rc2) return rc2;

    if (r1 == 0x00) return SD_OK; // ready
    delay_ms(20);
  }
  return SD_ERR_TIMEOUT;
}

static uint8_t sd_read_ocr_and_capacity(spi_t* bus){
  uint8_t r1 = 0xFF, rc;

  rc = sd_cmd_r1(bus, 58, 0, 0xFF, &r1);
  if (rc) { sd_cs_release(bus); return rc; }
  if (r1 != 0x00 && r1 != 0x01){ sd_cs_release(bus); return SD_ERR_BAD_R1; }

  uint8_t ocr[4];
  rc = sd_spi_recv_bytes(bus, ocr, 4);
  uint8_t rc2 = sd_cs_release(bus);
  if (rc) return rc;
  if (rc2) return rc2;

  g_is_sdhc = (ocr[0] & 0x40) ? 1 : 0;
  return SD_OK;
}

uint8_t sd_init(spi_t* bus){
  uint8_t rc;

  printf("[SD] idle clocks\r\n");
  rc = sd_clock_idle(bus, 80);
  printf("[SD] idle rc=%02X\r\n", rc);
  if (rc) return rc;

  printf("[SD] CMD0\r\n");
  rc = sd_go_idle(bus);
  printf("[SD] CMD0 rc=%02X\r\n", rc);
  if (rc) return rc;

  printf("[SD] CMD8\r\n");
  (void)sd_check_if_v2_and_voltage_ok(bus, NULL);
  printf("[SD] CMD8 done\r\n");

  printf("[SD] ACMD41\r\n");
  rc = sd_send_acmd41_hcs(bus);
  printf("[SD] ACMD41 rc=%02X\r\n", rc);
  if (rc) return rc;

  printf("[SD] CMD58\r\n");
  rc = sd_read_ocr_and_capacity(bus);
  printf("[SD] CMD58 rc=%02X\r\n", rc);
  if (rc) return rc;

  return SD_OK;
}

static inline uint32_t sd_arg_addr(uint32_t lba){
  return g_is_sdhc ? lba : (lba * 512u);
}

uint8_t sd_read_block(spi_t* bus, uint32_t lba, uint8_t *dst512){
  uint8_t r1 = 0xFF, rc;

  rc = sd_cmd_r1(bus, 17, sd_arg_addr(lba), 0xFF, &r1);
  if (rc) { sd_cs_release(bus); return rc; }
  if (r1 != 0x00){ sd_cs_release(bus); return SD_ERR_BAD_R1; }

  rc = sd_wait_token(bus, 0xFE, bus->token_timeout);
  if (rc) { sd_cs_release(bus); return rc; }

  // Read 512 data + 2 CRC
  rc = sd_spi_recv_bytes(bus, dst512, 512);
  if (rc) { sd_cs_release(bus); return rc; }

  uint8_t dummy;
  rc  = sd_spi_recv(bus, &dummy);
  rc |= sd_spi_recv(bus, &dummy);
  if (rc) { sd_cs_release(bus); return rc; }

  return sd_cs_release(bus);
}

uint8_t sd_write_block(spi_t* bus, uint32_t lba, const uint8_t *src512){
  uint8_t r1 = 0xFF, rc;

  rc = sd_cmd_r1(bus, 24, sd_arg_addr(lba), 0xFF, &r1);
  if (rc) { sd_cs_release(bus); return rc; }
  if (r1 != 0x00){ sd_cs_release(bus); return SD_ERR_BAD_R1; }

  rc = sd_spi_send(bus, 0xFF); if (rc) { sd_cs_release(bus); return rc; } // stuff
  rc = sd_spi_send(bus, 0xFE); if (rc) { sd_cs_release(bus); return rc; } // start token

  rc = sd_spi_send_bytes(bus, src512, 512);
  if (rc) { sd_cs_release(bus); return rc; }

  // dummy CRC
  rc  = sd_spi_send(bus, 0xFF);
  rc |= sd_spi_send(bus, 0xFF);
  if (rc) { sd_cs_release(bus); return rc; }

  // Data response: 0bxxx00101 => accepted
  uint8_t resp = 0xFF;
  rc = sd_spi_recv(bus, &resp);
  if (rc) { sd_cs_release(bus); return rc; }
  if ((resp & 0x1F) != 0x05){ sd_cs_release(bus); return SD_ERR_RESP; }

  // Wait not busy (card drives MISO low while programming)
  uint32_t t = bus->token_timeout;
  while (t--) {
    uint8_t b = 0x00;
    rc = sd_spi_recv(bus, &b);
    if (rc) { sd_cs_release(bus); return rc; }
    if (b == 0xFF) break;
    delay_ms(1);
  }
  if ((int32_t)t <= 0) { sd_cs_release(bus); return SD_ERR_TIMEOUT; }

  return sd_cs_release(bus);
}
