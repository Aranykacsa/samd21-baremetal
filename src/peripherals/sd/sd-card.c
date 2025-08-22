#include "sd-card.h"
#include "samd21.h"
#include <stddef.h> 

#include <stdio.h>

/* -------- Pin map (SERCOM1: DOPO=0 SCK on PAD1, MOSI PAD0, MISO DIPO=3) ----- */
#define SD_MOSI_PORT 0
#define SD_MOSI_PIN  16  /* PA16 = SERCOM1 PAD0 */
#define SD_SCK_PORT  0
#define SD_SCK_PIN   17  /* PA17 = SERCOM1 PAD1 */
#define SD_MISO_PORT 0
#define SD_MISO_PIN  19  /* PA19 = SERCOM1 PAD3 */
#define SD_CS_PORT   0
#define SD_CS_PIN    18  /* PA18 = GPIO chip-select */

#define SD_CS_HIGH() (PORT->Group[SD_CS_PORT].OUTSET.reg = (1u << SD_CS_PIN))
#define SD_CS_LOW()  (PORT->Group[SD_CS_PORT].OUTCLR.reg = (1u << SD_CS_PIN))

/* -------- Config ------------------------------------------------------------ */
#ifndef SD_SPI_INIT_HZ
#define SD_SPI_INIT_HZ 400000u     /* ~400 kHz during init */
#endif
#ifndef SD_SPI_RUN_HZ
#define SD_SPI_RUN_HZ  8000000u    /* 8 MHz after init */
#endif
#ifndef SD_CMD_TIMEOUT
#define SD_CMD_TIMEOUT 100u        /* ms */
#endif
#ifndef SD_TOKEN_TIMEOUT
#define SD_TOKEN_TIMEOUT 100u      /* ms */
#endif

/* -------- Locals ------------------------------------------------------------ */
static int g_is_sdhc = 0;

/* crude delay (busy-wait) using 48 MHz core */
static void delay_us(uint32_t us) {
  /* ~6 cycles per loop (very rough); tune if needed */
  volatile uint32_t cycles = (SystemCoreClock / 6000000u) * us;
  while (cycles--) __asm__ volatile("nop");
}

static void delay_ms(uint32_t ms){ while (ms--) delay_us(1000); }

/* -------- SERCOM1 SPI low-level -------------------------------------------- */

static inline void sercom1_wait_sync(void) {
  while (SERCOM1->SPI.SYNCBUSY.reg);
}

static uint8_t spi_txrx(uint8_t v) {
  while (!SERCOM1->SPI.INTFLAG.bit.DRE);
  SERCOM1->SPI.DATA.reg = v;
  while (!SERCOM1->SPI.INTFLAG.bit.RXC);
  return (uint8_t)SERCOM1->SPI.DATA.reg;
}

static void spi_chip_select_init(void) {
  PM->APBBMASK.bit.PORT_ = 1;
  /* CS as output, high */
  PORT->Group[SD_CS_PORT].DIRSET.reg = (1u << SD_CS_PIN);
  SD_CS_HIGH();
}

static void spi_pins_init(void) {
  /* PA16/PA17/PA19 to peripheral C (SERCOM1) */
  PM->APBBMASK.bit.PORT_ = 1;
  /* MOSI PA16 = even index -> PMUXE */
  PORT->Group[0].PINCFG[SD_MOSI_PIN].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[SD_MOSI_PIN >> 1].bit.PMUXE = PORT_PMUX_PMUXE_C_Val;

  /* SCK PA17 = odd index -> PMUXO */
  PORT->Group[0].PINCFG[SD_SCK_PIN].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[SD_SCK_PIN >> 1].bit.PMUXO = PORT_PMUX_PMUXO_C_Val;

  /* MISO PA19 = odd index -> PMUXO */
  PORT->Group[0].PINCFG[SD_MISO_PIN].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[SD_MISO_PIN >> 1].bit.PMUXO = PORT_PMUX_PMUXO_C_Val;
}

static void spi_enable_gclk(void) {
  /* Power & clocks for SERCOM1 */
  PM->APBCMASK.bit.SERCOM1_ = 1;
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM1_GCLK_ID_CORE) |
                      GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
}

static void spi_set_baud(uint32_t hz) {
  /* BAUD = fref / (2*hz) - 1, with fref = 48 MHz (GCLK0) */
  uint32_t fref = 48000000u;
  uint32_t baud = (fref / (2u * hz)) - 1u;
  if (baud > 255u) baud = 255u;
  SERCOM1->SPI.BAUD.reg = (uint8_t)baud;
  sercom1_wait_sync();
}

void sd_spi_set_hz(uint32_t hz){ spi_set_baud(hz); }

static void spi_master_init(uint32_t hz) {
  spi_enable_gclk();
  spi_pins_init();
  spi_chip_select_init();

  /* Reset */
  SERCOM1->SPI.CTRLA.bit.ENABLE = 0;
  sercom1_wait_sync();
  SERCOM1->SPI.CTRLA.bit.SWRST = 1;
  while (SERCOM1->SPI.CTRLA.bit.SWRST || SERCOM1->SPI.SYNCBUSY.bit.SWRST);

  /* Master, CPOL=0, CPHA=0 (mode 0), DOPO=0 (MOSI PAD0, SCK PAD1), DIPO=3 (MISO PAD3), MSB first */
  SERCOM1->SPI.CTRLA.reg =
      SERCOM_SPI_CTRLA_MODE(3)   /* Master */
    | SERCOM_SPI_CTRLA_DOPO(0)   /* MOSI=PAD0, SCK=PAD1 */
    | SERCOM_SPI_CTRLA_DIPO(3);  /* MISO=PAD3 */


  /* Enable receiver */
  SERCOM1->SPI.CTRLB.reg = SERCOM_SPI_CTRLB_RXEN;
  sercom1_wait_sync();

  spi_set_baud(hz);

  /* Enable */
  SERCOM1->SPI.CTRLA.bit.ENABLE = 1;
  sercom1_wait_sync();

  /* Prime the bus with a dummy read */
  (void)SERCOM1->SPI.DATA.reg;
}

/* -------- SD SPI helpers ---------------------------------------------------- */

static uint8_t sd_spi_recv(void){ return spi_txrx(0xFF); }
static void    sd_spi_send(uint8_t v){ spi_txrx(v); }

static void sd_spi_send_bytes(const uint8_t *p, uint32_t n){
  while (n--) sd_spi_send(*p++);
}
static void sd_spi_recv_bytes(uint8_t *p, uint32_t n){
  while (n--) *p++ = sd_spi_recv();
}

static void sd_clock_idle(uint32_t clocks){
  /* SD spec: at least 74 clocks with CS high */
  SD_CS_HIGH();
  for (uint32_t i = 0; i < clocks/8u; i++) sd_spi_send(0xFF);
}

static uint8_t sd_wait_r1(uint32_t ms){
  /* Wait until a non-0xFF appears (R1), or timeout */
  uint32_t t = ms;
  while (t--){
    uint8_t r = sd_spi_recv();
    if ((r & 0x80) == 0) return r;
    delay_ms(1);
  }
  return 0xFF;
}

static int sd_wait_token(uint8_t token, uint32_t ms){
  uint32_t t = ms;
  while (t--){
    uint8_t b = sd_spi_recv();
    if (b == token) return 0;
    delay_ms(1);
  }
  return -1;
}

/* Send a command: cmd = 0..63 (without 0x40), arg big-endian, returns R1 */
static uint8_t sd_cmd_r1(uint8_t cmd, uint32_t arg, uint8_t crc){
  uint8_t frame[6];
  frame[0] = 0x40 | (cmd & 0x3F);
  frame[1] = (uint8_t)(arg >> 24);
  frame[2] = (uint8_t)(arg >> 16);
  frame[3] = (uint8_t)(arg >> 8);
  frame[4] = (uint8_t)arg;
  frame[5] = crc;

  SD_CS_LOW();
  sd_spi_send(0xFF);
  sd_spi_send_bytes(frame, 6);

  uint8_t r1 = sd_wait_r1(SD_CMD_TIMEOUT);
  return r1;
}


/* Some commands have extra response bytes; caller reads them while CS low, then high */
static void sd_cs_release(void){
  SD_CS_HIGH();
  /* One extra clock after CS high to flush the bus */
  sd_spi_recv();
}

/* -------- Public API -------------------------------------------------------- */

int sd_is_sdhc(void){ return g_is_sdhc; }

static int sd_go_idle(void){
  /* CMD0 with proper CRC = 0x95 */
  for (int i=0;i<10;i++){
    uint8_t r1 = sd_cmd_r1(0, 0, 0x95);
    if (r1 == 0x01) { sd_cs_release(); return 0; } /* in idle state */
    sd_cs_release();
    delay_ms(10);
  }
  return -1;
}

static int sd_check_if_v2_and_voltage_ok(uint32_t *ocr_out){
  /* CMD8 with pattern 0xAA and VHS=0x1 (2.7-3.6V); CRC = 0x87 */
  uint8_t r1 = sd_cmd_r1(8, 0x000001AAu, 0x87);
  if (r1 & 0x04) { sd_cs_release(); return -2; } /* illegal command -> v1.x card */
  if (r1 != 0x01){ sd_cs_release(); return -1; } /* expect idle */

  uint8_t r7[4];
  sd_spi_recv_bytes(r7, 4);
  sd_cs_release();

  /* Check echo pattern */
  if (r7[3] != 0xAA) return -3;

  if (ocr_out) *ocr_out = (r7[0]<<24)|(r7[1]<<16)|(r7[2]<<8)|r7[3];
  return 0;
}

static int sd_send_acmd41_hcs(void){
  /* ACMD41: first CMD55 then CMD41 with HCS=1 */
  for (uint32_t ms=0; ms<1000; ms+=20){
    uint8_t r1 = sd_cmd_r1(55, 0, 0xFF);  /* APP_CMD */
    sd_cs_release();
    if (r1 > 0x01) return -1;

    r1 = sd_cmd_r1(41, 0x40000000u, 0xFF); /* HCS bit */
    sd_cs_release();
    if (r1 == 0x00) return 0;               /* ready */
    delay_ms(20);
  }
  return -2; /* timeout */
}

static int sd_read_ocr_and_capacity(void){
  uint8_t r1 = sd_cmd_r1(58, 0, 0xFF);
  if (r1 > 0x01 && r1 != 0x00){ sd_cs_release(); return -1; }

  uint8_t ocr[4];
  for (int i=0; i<4; i++) {
    ocr[i] = sd_spi_recv();
  }
  sd_cs_release();

  g_is_sdhc = (ocr[0] & 0x40) ? 1 : 0;
  return 0;
}


int sd_init(void){
  /* Bring up SPI @ 400 kHz */
  spi_master_init(SD_SPI_INIT_HZ);

  /* 80 clocks with CS high */
  sd_clock_idle(80);

  /* CMD0 → idle */
  if (sd_go_idle() != 0) return -10;

  /* CMD8 (v2 check) — if illegal, try ACMD41 without HCS, but we’ll still default to byte addressing */
  int v2ok = sd_check_if_v2_and_voltage_ok(NULL);
  /* Loop ACMD41 until ready */
  if (sd_send_acmd41_hcs() != 0) return -11;
  /* CMD58: read OCR → CCS */
  if (sd_read_ocr_and_capacity() != 0) return -12;
  /* If not SDHC, ensure block length 512 (CMD16) */
  if (!g_is_sdhc){
    uint8_t r1 = sd_cmd_r1(16, 512, 0xFF);
    sd_cs_release();
    if (r1 != 0x00) return -13;
  }

  /* One extra idle byte before first data command */
  SD_CS_HIGH(); sd_spi_send(0xFF);

  (void)v2ok; /* info only */
  return 0;
}

/* Address argument handling */
static inline uint32_t sd_arg_addr(uint32_t lba){
  return g_is_sdhc ? lba : (lba * 512u);
}

int sd_read_block(uint32_t lba, uint8_t *dst512){
  uint8_t r1 = sd_cmd_r1(17, sd_arg_addr(lba), 0xFF);
  if (r1 != 0x00){ sd_cs_release(); return -1; }

  if (sd_wait_token(0xFE, SD_TOKEN_TIMEOUT) != 0){
    sd_cs_release(); return -2;
  }

  for (int i=0; i<16; i++) {
    dst512[i] = sd_spi_recv();
  }
  // read the rest
  sd_spi_recv_bytes(dst512+16, 512-16);
  (void)sd_spi_recv(); (void)sd_spi_recv();

  sd_cs_release();
  return 0;
}


int sd_write_block(uint32_t lba, const uint8_t *src512){
  uint8_t r1 = sd_cmd_r1(24, sd_arg_addr(lba), 0xFF);
  if (r1 != 0x00){ sd_cs_release(); return -1; }

  /* One stuff byte before token is okay */
  sd_spi_send(0xFF);

  /* Start token 0xFE, then 512 data, then 2-byte CRC (dummy) */
  sd_spi_send(0xFE);
  sd_spi_send_bytes(src512, 512);
  sd_spi_send(0xFF); sd_spi_send(0xFF);

  /* Read data response: 0bxxx00101 means accepted */
  uint8_t resp = sd_spi_recv();
  if ((resp & 0x1F) != 0x05){ sd_cs_release(); return -2; }

  /* Wait for not busy (0xFF) */
  uint32_t t = SD_TOKEN_TIMEOUT;
  while (t--){
    if (sd_spi_recv() == 0xFF) break;
    delay_ms(1);
  }
  if (!t){ sd_cs_release(); return -3; }

  sd_cs_release();
  return 0;
}