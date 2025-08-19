// samd21_aht20_sercom3.c
#include "samd21.h"
#include "system_samd21.h"
#include <stdint.h>
#include <stdio.h>

/* ==================== SysTick + delays (YOUR VERSION) ==================== */
static volatile uint32_t g_ms_ticks = 0;

void SysTick_Handler(void) { g_ms_ticks++; }

/* 1 kHz SysTick */
static inline void systick_init(void) {
  SysTick_Config(SystemCoreClock / 1000u);
}

/* Millisecond delay (rollover-safe) */
static inline void delay_ms(uint32_t ms) {
  uint32_t start = g_ms_ticks;
  while ((uint32_t)(g_ms_ticks - start) < ms) { /* busy wait */ }
}

/* Microsecond delay (coarse, cycles) */
static inline void delay_us(uint32_t us) {
  uint32_t cycles = (SystemCoreClock / 1000000u) * us / 5u; // rough tuning
  while (cycles--) { __asm__ volatile ("nop"); }
}

/* ==================== SERCOM3 I2C on PA22/PA23 (100 kHz) ==================== */
/* Pins: SDA=PA22, SCL=PA23, PMUX function C */
static void i2c3_init_100k(void) {
  /* APBC clock to SERCOM3 */
  PM->APBCMASK.reg |= PM_APBCMASK_SERCOM3;

  /* Route GCLK0 (48 MHz) to SERCOM3 core */
  GCLK->CLKCTRL.reg =
      GCLK_CLKCTRL_ID(SERCOM3_GCLK_ID_CORE) |
      GCLK_CLKCTRL_GEN_GCLK0 |
      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  /* PORT clock + pinmux */
  PM->APBBMASK.reg |= PM_APBBMASK_PORT;
  PORT->Group[0].PINCFG[22].bit.PMUXEN = 1;
  PORT->Group[0].PINCFG[23].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[22/2].bit.PMUXE = PORT_PMUX_PMUXE_C_Val; // PA22 SDA
  PORT->Group[0].PMUX[23/2].bit.PMUXO = PORT_PMUX_PMUXO_C_Val; // PA23 SCL

  /* Reset SERCOM3 I2C master */
  SERCOM3->I2CM.CTRLA.bit.SWRST = 1;
  while (SERCOM3->I2CM.SYNCBUSY.bit.SWRST);

  /* Master mode, SDAHOLD≈600ns, Standard/Fast, Smart mode */
  SERCOM3->I2CM.CTRLA.reg =
      SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
      SERCOM_I2CM_CTRLA_SDAHOLD(0x2) |
      SERCOM_I2CM_CTRLA_SPEED(0x0) |     // Standard/Fast
      SERCOM_I2CM_CTRLA_SCLSM;
  SERCOM3->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;  // Smart mode
  while (SERCOM3->I2CM.SYNCBUSY.reg);

  /* BAUD for 100 kHz: BAUD = (Fref / (2*Fscl)) - 5  -> ~235 @ 48MHz */
  uint32_t baud = (SystemCoreClock / (2u * 100000u)) - 5u;
  SERCOM3->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(baud & 0xFF);

  /* Enable */
  SERCOM3->I2CM.CTRLA.bit.ENABLE = 1;
  while (SERCOM3->I2CM.SYNCBUSY.bit.ENABLE);

  /* Force IDLE state */
  SERCOM3->I2CM.STATUS.bit.BUSSTATE = 1; // IDLE
  while (SERCOM3->I2CM.SYNCBUSY.bit.SYSOP);
}

/* ---- wait helpers with YOUR millisecond ticker ---- */
static inline int i2c3_wait_mb(uint32_t to_ms) {
  uint32_t t0 = g_ms_ticks;
  while (!SERCOM3->I2CM.INTFLAG.bit.MB) {
    if (SERCOM3->I2CM.STATUS.bit.BUSERR)   return -1;
    if (SERCOM3->I2CM.STATUS.bit.ARBLOST)  return -2;
    if ((uint32_t)(g_ms_ticks - t0) > to_ms) return -3;
  }
  return 0;
}
static inline int i2c3_wait_sb(uint32_t to_ms) {
  uint32_t t0 = g_ms_ticks;
  while (!SERCOM3->I2CM.INTFLAG.bit.SB) {
    if (SERCOM3->I2CM.STATUS.bit.BUSERR)   return -1;
    if (SERCOM3->I2CM.STATUS.bit.ARBLOST)  return -2;
    if ((uint32_t)(g_ms_ticks - t0) > to_ms) return -3;
  }
  return 0;
}

/* Write buf with STOP. 0=OK */
static int i2c3_write(uint8_t addr7, const uint8_t *buf, uint32_t len) {
  SERCOM3->I2CM.ADDR.reg = (addr7 << 1) | 0;     // W
  if (i2c3_wait_mb(5) != 0) return 1;
  if (SERCOM3->I2CM.STATUS.bit.RXNACK) {        // NACK on address
    SERCOM3->I2CM.CTRLB.bit.CMD = 3;            // STOP
    return 2;
  }
  for (uint32_t i=0;i<len;i++) {
    SERCOM3->I2CM.DATA.reg = buf[i];
    if (i2c3_wait_mb(5) != 0) return 3;
    if (SERCOM3->I2CM.STATUS.bit.RXNACK) {
      SERCOM3->I2CM.CTRLB.bit.CMD = 3;
      return 4;
    }
  }
  SERCOM3->I2CM.CTRLB.bit.CMD = 3;              // STOP
  return 0;
}

/* Read buf with STOP. 0=OK */
static int i2c3_read(uint8_t addr7, uint8_t *buf, uint32_t len) {
  if (len == 0) return 0;
  SERCOM3->I2CM.ADDR.reg = (addr7 << 1) | 1;    // R
  if (i2c3_wait_sb(5) != 0) return 1;

  for (uint32_t i=0;i<len;i++) {
    if (i2c3_wait_sb(5) != 0) return 2;
    if (i == (len - 1)) {
      SERCOM3->I2CM.CTRLB.bit.ACKACT = 1;       // NACK last byte
      SERCOM3->I2CM.CTRLB.bit.CMD    = 3;       // STOP
    }
    buf[i] = SERCOM3->I2CM.DATA.reg;           // Smart mode: DATA read advances
  }
  return 0;
}

/* ==================== AHT20 (blocking, linear) ==================== */
static uint8_t aht20_init(uint8_t addr7) {
  const uint8_t seq[3] = {0xBE,0x08,0x00};
  int rc = i2c3_write(addr7, seq, 3);
  delay_ms(100);                                // one-time calibration
  return (rc == 0) ? 0 : 1;
}

static uint8_t aht20_read(uint8_t addr7, float *temp_c, float *hum_pct) {
  const uint8_t trig[3] = {0xAC,0x33,0x00};
  if (i2c3_write(addr7, trig, 3) != 0) return 1;

  delay_ms(90);                                 // conservative conversion wait

  uint8_t d[6];
  if (i2c3_read(addr7, d, 6) != 0) return 2;

  if (d[0] & 0x80) {                            // still busy (rare)
    delay_ms(10);
    if (i2c3_read(addr7, d, 6) != 0) return 3;
    if (d[0] & 0x80) return 4;
  }

  /* Correct 20-bit extraction */
  uint32_t humRaw  = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t tempRaw = ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];

  *hum_pct = (humRaw  * 100.0f) / 1048576.0f;
  *temp_c  = (tempRaw * 200.0f) / 1048576.0f - 50.0f;
  return 0;
}

/* ==================== Demo main ==================== */
extern void uart_init_115200_sercom5_pa10_pa11(void); // remove if not used

int main(void) {
<<<<<<< HEAD
  SystemInit();               // make sure you’re really at 48 MHz
  setvbuf(stdout, NULL, _IONBF, 0);   // unbuffered stdout
  setvbuf(stderr, NULL, _IONBF, 0);
  i2c_begin(100000);          // SERCOM3 on PA22/PA23 (default in i2c_wire.h)
  uart_init(SERCOM5, 115200, 10, 1, 11, 3, PORT_PMUX_PMUXE_D_Val);
=======
  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);     // 1 ms tick

  /* LED PA17 for heartbeat (Feather M0 D13) */
  PM->APBBMASK.reg |= PM_APBBMASK_PORT;
  PORT->Group[0].DIRSET.reg = (1u << 17);

  /* (Optional) UART for printf */
  uart_init_115200_sercom5_pa10_pa11();
  setvbuf(stdout, NULL, _IONBF, 0);

  i2c_begin(100000);                          // your SERCOM/pins for I2C
>>>>>>> b1a797da161d67d361500eb9fa39ba28b01a54bb

  printf("Hello from Feather M0 @115200 8N1\n");

  // one-time init sequence (matches your working Wire code)
  if (aht20_init(0x38) != 0) {
    printf("AHT20 not responding during init.\n");
  } else {
    printf("AHT20 init OK\r\n");
  }

  float t, h;
  uint32_t next = g_ms_ticks;

  while (1) {
    if ((int32_t)(g_ms_ticks - next) >= 0) {
      next += 1000;                  // 1 Hz

      uint8_t rc = aht20_read(AHT_ADDR, &t, &h);
      if (rc == 0) {
        printf("T=%.1f C  RH=%.1f %%\r\n", (double)t, (double)h);
      } else {
        printf("AHT20 read err=%u\r\n", rc);
      }

      PORT->Group[0].OUTTGL.reg = (1u << 17);
    }
  }
}
