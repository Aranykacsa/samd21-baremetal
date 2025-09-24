#include "i2c-helper.h"
#include "samd21.h"
#include "system_samd21.h"
#include "variables.h"
#include "clock.h"     // must provide get_uptime() in ms
#include <stdint.h>
#include <stddef.h>
#include <stdint.h>

void enable_sercom_clock(Sercom* sercom, uint8_t* core, uint8_t* slow) {
  if (sercom == SERCOM0) { PM->APBCMASK.bit.SERCOM0_ = 1; *core = SERCOM0_GCLK_ID_CORE; *slow = SERCOM0_GCLK_ID_SLOW; }
  else if (sercom == SERCOM1) { PM->APBCMASK.bit.SERCOM1_ = 1; *core = SERCOM1_GCLK_ID_CORE; *slow = SERCOM1_GCLK_ID_SLOW; }
  else if (sercom == SERCOM2) { PM->APBCMASK.bit.SERCOM2_ = 1; *core = SERCOM2_GCLK_ID_CORE; *slow = SERCOM2_GCLK_ID_SLOW; }
  else if (sercom == SERCOM3) { PM->APBCMASK.bit.SERCOM3_ = 1; *core = SERCOM3_GCLK_ID_CORE; *slow = SERCOM3_GCLK_ID_SLOW; }
  else if (sercom == SERCOM4) { PM->APBCMASK.bit.SERCOM4_ = 1; *core = SERCOM4_GCLK_ID_CORE; *slow = SERCOM4_GCLK_ID_SLOW; }
  else if (sercom == SERCOM5) { PM->APBCMASK.bit.SERCOM5_ = 1; *core = SERCOM5_GCLK_ID_CORE; *slow = SERCOM5_GCLK_ID_SLOW; }
}

void i2c_apply_baud(Sercom* sercom, uint32_t fscl_hz,
                                  uint32_t src_hz, uint32_t trise_ns) {
  if (fscl_hz == 0) fscl_hz = I2C_DEFAULT_HZ;

  uint32_t baud = (src_hz / (2u * fscl_hz));
  if (baud > 5) baud -= 5; else baud = 0;

  if (trise_ns) {
    uint64_t comp = ((uint64_t)src_hz * (uint64_t)trise_ns) / 2000000000ull;
    if (baud > comp) baud -= (uint32_t)comp; else baud = 0;
  }

  if (baud > 255) baud = 255;
  sercom->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(baud);
}

void i2c_issue_stop(i2c_t* bus) {
  bus->sercom->I2CM.CTRLB.bit.CMD = 3;                    
  while (bus->sercom->I2CM.SYNCBUSY.bit.SYSOP);
  if (bus->sercom->I2CM.STATUS.bit.BUSERR)  bus->sercom->I2CM.STATUS.bit.BUSERR  = 1;
  if (bus->sercom->I2CM.STATUS.bit.ARBLOST) bus->sercom->I2CM.STATUS.bit.ARBLOST = 1;
}

uint8_t i2c_wait_mb(i2c_t* bus, uint32_t to_ms) {
  uint32_t t0 = get_uptime();
  while (!bus->sercom->I2CM.INTFLAG.bit.MB) {
    if (bus->sercom->I2CM.STATUS.bit.BUSERR)  { bus->sercom->I2CM.STATUS.bit.BUSERR  = 1; return -1; }
    if (bus->sercom->I2CM.STATUS.bit.ARBLOST) { bus->sercom->I2CM.STATUS.bit.ARBLOST = 1; return -2; }
    if ((uint32_t)(get_uptime() - t0) > to_ms) return -3;
  }
  return 0;
}

uint8_t i2c_wait_sb(i2c_t* bus, uint32_t to_ms) {
  uint32_t t0 = get_uptime();
  while (!bus->sercom->I2CM.INTFLAG.bit.SB) {
    if (bus->sercom->I2CM.STATUS.bit.BUSERR)  { bus->sercom->I2CM.STATUS.bit.BUSERR  = 1; return -1; }
    if (bus->sercom->I2CM.STATUS.bit.ARBLOST) { bus->sercom->I2CM.STATUS.bit.ARBLOST = 1; return -2; }
    if ((uint32_t)(get_uptime() - t0) > to_ms) return -3;
  }
  return 0;
}

void pmux_set(uint8_t port, uint8_t pin, uint8_t func) {
  PM->APBBMASK.reg |= PM_APBBMASK_PORT;
  PortGroup* grp = &PORT->Group[port];
  grp->PINCFG[pin].bit.PMUXEN = 1;
  grp->PINCFG[pin].bit.INEN   = 1;
  uint8_t idx = pin >> 1;
  if ((pin & 1) == 0) grp->PMUX[idx].bit.PMUXE = func;
  else               grp->PMUX[idx].bit.PMUXO = func;
}