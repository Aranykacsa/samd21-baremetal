// src/drivers/i2c/i2c.c
#include "i2c.h"
#include "samd21.h"
#include "system_samd21.h"

volatile uint32_t g_ms_ticks; // declared extern in header; defined in your app
#ifndef I2C_DEFAULT_HZ
#define I2C_DEFAULT_HZ 100000u
#endif

static uint32_t g_i2c_hz = I2C_DEFAULT_HZ;

/* -------- internal helpers -------- */
static inline void i2c_issue_stop(void) {
  SERCOM3->I2CM.CTRLB.bit.CMD = 3;                 // STOP
  while (SERCOM3->I2CM.SYNCBUSY.bit.SYSOP);        // wait bus op done
  if (SERCOM3->I2CM.STATUS.bit.BUSERR)  SERCOM3->I2CM.STATUS.bit.BUSERR  = 1;
  if (SERCOM3->I2CM.STATUS.bit.ARBLOST) SERCOM3->I2CM.STATUS.bit.ARBLOST = 1;
}

static inline int i2c_wait_mb(uint32_t to_ms) {
  uint32_t t0 = g_ms_ticks;
  while (!SERCOM3->I2CM.INTFLAG.bit.MB) {
    if (SERCOM3->I2CM.STATUS.bit.BUSERR)  { SERCOM3->I2CM.STATUS.bit.BUSERR  = 1; return -1; }
    if (SERCOM3->I2CM.STATUS.bit.ARBLOST) { SERCOM3->I2CM.STATUS.bit.ARBLOST = 1; return -2; }
    if ((uint32_t)(g_ms_ticks - t0) > to_ms) return -3;
  }
  return 0;
}

static inline int i2c_wait_sb(uint32_t to_ms) {
  uint32_t t0 = g_ms_ticks;
  while (!SERCOM3->I2CM.INTFLAG.bit.SB) {
    if (SERCOM3->I2CM.STATUS.bit.BUSERR)  { SERCOM3->I2CM.STATUS.bit.BUSERR  = 1; return -1; }
    if (SERCOM3->I2CM.STATUS.bit.ARBLOST) { SERCOM3->I2CM.STATUS.bit.ARBLOST = 1; return -2; }
    if ((uint32_t)(g_ms_ticks - t0) > to_ms) return -3;
  }
  return 0;
}

static void i2c_apply_baud(uint32_t hz) {
  if (hz == 0) hz = I2C_DEFAULT_HZ;
  g_i2c_hz = hz;
  uint32_t baud = (SystemCoreClock / (2u * hz)) - 5u;
  if (baud > 255u) baud = 255u;
  SERCOM3->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(baud & 0xFF);
}

/* -------- public helpers -------- */
void i2c_force_idle(void) {
  if (SERCOM3->I2CM.STATUS.bit.BUSERR)  SERCOM3->I2CM.STATUS.bit.BUSERR  = 1;
  if (SERCOM3->I2CM.STATUS.bit.ARBLOST) SERCOM3->I2CM.STATUS.bit.ARBLOST = 1;
  if (SERCOM3->I2CM.STATUS.bit.BUSSTATE != 1) {
    SERCOM3->I2CM.STATUS.bit.BUSSTATE = 1; // IDLE
    while (SERCOM3->I2CM.SYNCBUSY.bit.SYSOP);
  }
}

int i2c_set_speed(uint32_t hz) {
  i2c_apply_baud(hz);
  return 0;
}

/* -------- init / reinit -------- */
int i2c_init(uint32_t hz) {
  PM->APBCMASK.reg |= PM_APBCMASK_SERCOM3;

  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM3_GCLK_ID_CORE) |
                      GCLK_CLKCTRL_GEN_GCLK0 |
                      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  PM->APBBMASK.reg |= PM_APBBMASK_PORT;
  PORT->Group[0].PINCFG[22].bit.PMUXEN = 1; // SDA
  PORT->Group[0].PINCFG[23].bit.PMUXEN = 1; // SCL
  PORT->Group[0].PMUX[22/2].bit.PMUXE = PORT_PMUX_PMUXE_C_Val; // PA22 -> C
  PORT->Group[0].PMUX[23/2].bit.PMUXO = PORT_PMUX_PMUXO_C_Val; // PA23 -> C

  SERCOM3->I2CM.CTRLA.bit.SWRST = 1;
  while (SERCOM3->I2CM.SYNCBUSY.bit.SWRST);

  SERCOM3->I2CM.CTRLA.reg =
      SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
      SERCOM_I2CM_CTRLA_SDAHOLD(0x2) |
      SERCOM_I2CM_CTRLA_SPEED(0x0);        // Standard/Fast, SCLSM OFF
  SERCOM3->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN; // Smart mode
  while (SERCOM3->I2CM.SYNCBUSY.reg);

  i2c_apply_baud(hz);

  SERCOM3->I2CM.CTRLA.bit.ENABLE = 1;
  while (SERCOM3->I2CM.SYNCBUSY.bit.ENABLE);

  SERCOM3->I2CM.STATUS.bit.BUSSTATE = 1; // IDLE
  while (SERCOM3->I2CM.SYNCBUSY.bit.SYSOP);

  return 0;
}

void i2c_swrst_reinit(void) {
  SERCOM3->I2CM.CTRLA.bit.SWRST = 1;
  while (SERCOM3->I2CM.SYNCBUSY.bit.SWRST);

  SERCOM3->I2CM.CTRLA.reg =
      SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
      SERCOM_I2CM_CTRLA_SDAHOLD(0x2) |
      SERCOM_I2CM_CTRLA_SPEED(0x0);        // SCLSM OFF
  SERCOM3->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;
  while (SERCOM3->I2CM.SYNCBUSY.reg);

  i2c_apply_baud(g_i2c_hz);

  SERCOM3->I2CM.CTRLA.bit.ENABLE = 1;
  while (SERCOM3->I2CM.SYNCBUSY.bit.ENABLE);

  i2c_force_idle();
}

/* -------- transfers -------- */
int i2c_write(uint8_t addr7, const uint8_t *buf, uint32_t len) {
  i2c_force_idle();

  SERCOM3->I2CM.ADDR.reg = (addr7 << 1) | 0;     // W
  if (i2c_wait_mb(20) != 0) { i2c_issue_stop(); return 1; }
  if (SERCOM3->I2CM.STATUS.bit.RXNACK) { i2c_issue_stop(); return 2; }

  for (uint32_t i = 0; i < len; i++) {
    SERCOM3->I2CM.DATA.reg = buf[i];
    if (i2c_wait_mb(20) != 0) { i2c_issue_stop(); return 3; }
    if (SERCOM3->I2CM.STATUS.bit.RXNACK) { i2c_issue_stop(); return 4; }
  }
  i2c_issue_stop();
  return 0;
}

int i2c_read(uint8_t addr7, uint8_t *buf, uint32_t len) {
  if (len == 0) return 0;
  i2c_force_idle();

  SERCOM3->I2CM.ADDR.reg = (addr7 << 1) | 1;     // R
  if (i2c_wait_sb(20) != 0) { i2c_issue_stop(); return 1; }

  for (uint32_t i = 0; i < len; i++) {
    if (i2c_wait_sb(20) != 0) { i2c_issue_stop(); return 2; }
    if (i == (len - 1)) {
      SERCOM3->I2CM.CTRLB.bit.ACKACT = 1;       // NACK next
      buf[i] = SERCOM3->I2CM.DATA.reg;         // read -> NACK
      i2c_issue_stop();                        // STOP
    } else {
      SERCOM3->I2CM.CTRLB.bit.ACKACT = 0;      // ACK
      buf[i] = SERCOM3->I2CM.DATA.reg;
    }
  }
  return 0;
}
