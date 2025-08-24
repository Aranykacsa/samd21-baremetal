#include "samd21.h"
#include <stdint.h>
#include <stddef.h>
#include "clock.h"
#include <stdio.h>
#include "variables.h"
#include "spi.h"

/* Card type flag */
static int g_is_sdhc = 0;

/* Public setter if you want to bump speed after init */
void sd_spi_set_hz(spi_t* bus, uint32_t hz) { spi_set_baud(bus, hz); }

/* ==== SD SPI helpers ======================================================= */

static uint8_t sd_spi_recv(spi_t* bus){ return spi_txrx(bus, 0xFF); }
static void    sd_spi_send(spi_t* bus, uint8_t v){ (void)spi_txrx(bus, v); }

static void sd_spi_send_bytes(spi_t* bus, const uint8_t *p, uint32_t n){
  while (n--) sd_spi_send(bus, *p++);
}
static void sd_spi_recv_bytes(spi_t* bus, uint8_t *p, uint32_t n){
  while (n--) *p++ = sd_spi_recv(bus);
}

static void sd_clock_idle(spi_t* bus, uint32_t clocks){
  // SD spec: ≥74 clocks with CS high
  SD_CS_HIGH(bus);
  for (uint32_t i = 0; i < clocks/8u; i++) sd_spi_send(bus, 0xFF);
}

static uint8_t sd_wait_r1(spi_t* bus, uint32_t ms){
  while (ms--){
    uint8_t r = sd_spi_recv(bus);
    if ((r & 0x80) == 0) return r;
    delay_ms(1);
  }
  return 0xFF;
}

static int sd_wait_token(spi_t* bus, uint8_t token, uint32_t ms){
  while (ms--){
    uint8_t b = sd_spi_recv(bus);
    if (b == token) return 0;
    delay_ms(1);
  }
  return -1;
}

/* Send command: cmd=0..63 (no 0x40), arg big-endian, return R1 */
static uint8_t sd_cmd_r1(spi_t* bus, uint8_t cmd, uint32_t arg, uint8_t crc){
  uint8_t frame[6];
  frame[0] = 0x40 | (cmd & 0x3F);
  frame[1] = (uint8_t)(arg >> 24);
  frame[2] = (uint8_t)(arg >> 16);
  frame[3] = (uint8_t)(arg >> 8);
  frame[4] = (uint8_t)(arg);
  frame[5] = crc;

  SD_CS_LOW(bus);
  sd_spi_send(bus, 0xFF);                 // one stuff byte
  sd_spi_send_bytes(bus, frame, 6);

  uint8_t r1 = sd_wait_r1(bus, spi_s1.cmd_timeout);
  return r1;
}

static void sd_cs_release(spi_t* bus){
  SD_CS_HIGH(bus);
  (void)sd_spi_recv(bus); // extra clock
}

/* ==== SD public API ======================================================== */

int sd_is_sdhc(void){ return g_is_sdhc; }

static int sd_go_idle(spi_t* bus){
  // CMD0 with CRC 0x95
  for (int i = 0; i < 10; i++){
    uint8_t r1 = sd_cmd_r1(bus, 0, 0, 0x95);
    if (r1 == 0x01) { sd_cs_release(bus); return 0; }
    sd_cs_release(bus);
    delay_ms(10);
  }
  return -1;
}

static int sd_check_if_v2_and_voltage_ok(spi_t* bus, uint32_t *ocr_out){
  // CMD8 VHS=0x1, pattern 0xAA, CRC 0x87
  uint8_t r1 = sd_cmd_r1(bus, 8, 0x000001AAu, 0x87);
  if (r1 & 0x04) { sd_cs_release(bus); return -2; } // illegal -> v1.x
  if (r1 != 0x01){ sd_cs_release(bus); return -1; } // expect idle

  uint8_t r7[4];
  sd_spi_recv_bytes(bus, r7, 4);
  sd_cs_release(bus);

  if (r7[3] != 0xAA) return -3;
  if (ocr_out) *ocr_out = ((uint32_t)r7[0] << 24) | ((uint32_t)r7[1] << 16) | ((uint32_t)r7[2] << 8) | r7[3];
  return 0;
}

static int sd_send_acmd41_hcs(spi_t* bus){
  for (uint32_t ms = 0; ms < 1000; ms += 20){
    uint8_t r1 = sd_cmd_r1(bus, 55, 0, 0xFF);  // APP_CMD
    sd_cs_release(bus);
    if (r1 > 0x01) return -1;

    r1 = sd_cmd_r1(bus, 41, 0x40000000u, 0xFF); // HCS
    sd_cs_release(bus);
    if (r1 == 0x00) return 0;                    // ready
    delay_ms(20);
  }
  return -2;
}

static int sd_read_ocr_and_capacity(spi_t* bus){
  uint8_t r1 = sd_cmd_r1(bus, 58, 0, 0xFF);
  if (r1 > 0x01 && r1 != 0x00){ sd_cs_release(bus); return -1; }

  uint8_t ocr[4];
  sd_spi_recv_bytes(bus, ocr, 4);
  sd_cs_release(bus);

  g_is_sdhc = (ocr[0] & 0x40) ? 1 : 0;
  return 0;
}

int sd_init(spi_t* bus){
  // Bring up SPI at init speed
  spi_master_init(bus);

  // ≥80 clocks with CS high
  sd_clock_idle(bus, 80);

  // CMD0 → idle
  if (sd_go_idle(bus) != 0) return -10;

  // CMD8 (v2 check)
  (void)sd_check_if_v2_and_voltage_ok(bus, NULL);

  // ACMD41 with HCS until ready
  if (sd_send_acmd41_hcs(bus) != 0) return -11;

  // CMD58: read OCR → CCS
  if (sd_read_ocr_and_capacity(bus) != 0) return -12;

  // Non-SDHC: set block length 512 (CMD16)
  if (!g_is_sdhc){
    uint8_t r1 = sd_cmd_r1(bus, 16, 512, 0xFF);
    sd_cs_release(bus);
    if (r1 != 0x00) return -13;
  }

  // Switch to run speed
  sd_spi_set_hz(bus, bus->run_hz);

  // One extra idle byte before first data command
  SD_CS_HIGH(bus); sd_spi_send(bus, 0xFF);

  return 0;
}

static inline uint32_t sd_arg_addr(uint32_t lba){
  return g_is_sdhc ? lba : (lba * 512u);
}

int sd_read_block(spi_t* bus, uint32_t lba, uint8_t *dst512){
  uint8_t r1 = sd_cmd_r1(bus, 17, sd_arg_addr(lba), 0xFF);
  if (r1 != 0x00){ sd_cs_release(bus); return -1; }

  if (sd_wait_token(bus, 0xFE, bus->token_timeout) != 0){
    sd_cs_release(bus); return -2;
  }

  // Read 512 data + 2 CRC
  sd_spi_recv_bytes(bus, dst512, 512);
  (void)sd_spi_recv(bus); (void)sd_spi_recv(bus);

  sd_cs_release(bus);
  return 0;
}

int sd_write_block(spi_t* bus, uint32_t lba, const uint8_t *src512){
  uint8_t r1 = sd_cmd_r1(bus, 24, sd_arg_addr(lba), 0xFF);
  if (r1 != 0x00){ sd_cs_release(bus); return -1; }

  sd_spi_send(bus, 0xFF);           // stuff
  sd_spi_send(bus, 0xFE);           // start token
  sd_spi_send_bytes(bus, src512, 512);
  sd_spi_send(bus, 0xFF); sd_spi_send(bus, 0xFF); // dummy CRC

  // Data response: 0bxxx00101 => accepted
  uint8_t resp = sd_spi_recv(bus);
  if ((resp & 0x1F) != 0x05){ sd_cs_release(bus); return -2; }

  // Wait not busy
  uint32_t t = bus->token_timeout;
  while (t--){
    if (sd_spi_recv(bus) == 0xFF) break;
    delay_ms(1);
  }
  if ((int)t <= 0){ sd_cs_release(bus); return -3; }

  sd_cs_release(bus);
  return 0;
}