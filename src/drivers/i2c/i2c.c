#include "i2c.h"
#include "samd21.h"
#include "system_samd21.h"
#include "variables.h"
#include "clock.h"     // must provide get_uptime() in ms
#include <stdint.h>

#ifndef I2C_DEFAULT_HZ
#define I2C_DEFAULT_HZ 100000u
#endif

/* ---------- private helpers ---------- */

static inline void pmux_set(uint8_t port, uint8_t pin, uint8_t func) {
  PM->APBBMASK.reg |= PM_APBBMASK_PORT;
  PortGroup* grp = &PORT->Group[port];
  grp->PINCFG[pin].bit.PMUXEN = 1;
  grp->PINCFG[pin].bit.INEN   = 1;
  uint8_t idx = pin >> 1;
  if ((pin & 1) == 0) grp->PMUX[idx].bit.PMUXE = func;
  else               grp->PMUX[idx].bit.PMUXO = func;
}

static inline void enable_sercom_clock(Sercom* sercom, uint8_t* core, uint8_t* slow) {
  if (sercom == SERCOM0) { PM->APBCMASK.bit.SERCOM0_ = 1; *core = SERCOM0_GCLK_ID_CORE; *slow = SERCOM0_GCLK_ID_SLOW; }
  else if (sercom == SERCOM1) { PM->APBCMASK.bit.SERCOM1_ = 1; *core = SERCOM1_GCLK_ID_CORE; *slow = SERCOM1_GCLK_ID_SLOW; }
  else if (sercom == SERCOM2) { PM->APBCMASK.bit.SERCOM2_ = 1; *core = SERCOM2_GCLK_ID_CORE; *slow = SERCOM2_GCLK_ID_SLOW; }
  else if (sercom == SERCOM3) { PM->APBCMASK.bit.SERCOM3_ = 1; *core = SERCOM3_GCLK_ID_CORE; *slow = SERCOM3_GCLK_ID_SLOW; }
  else if (sercom == SERCOM4) { PM->APBCMASK.bit.SERCOM4_ = 1; *core = SERCOM4_GCLK_ID_CORE; *slow = SERCOM4_GCLK_ID_SLOW; }
  else if (sercom == SERCOM5) { PM->APBCMASK.bit.SERCOM5_ = 1; *core = SERCOM5_GCLK_ID_CORE; *slow = SERCOM5_GCLK_ID_SLOW; }
}

static inline void i2c_apply_baud(Sercom* sercom, uint32_t fscl_hz,
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

static inline void i2c_issue_stop(i2c_t* bus) {
  bus->sercom->I2CM.CTRLB.bit.CMD = 3;                    
  while (bus->sercom->I2CM.SYNCBUSY.bit.SYSOP);
  if (bus->sercom->I2CM.STATUS.bit.BUSERR)  bus->sercom->I2CM.STATUS.bit.BUSERR  = 1;
  if (bus->sercom->I2CM.STATUS.bit.ARBLOST) bus->sercom->I2CM.STATUS.bit.ARBLOST = 1;
}

static inline int i2c_wait_mb(i2c_t* bus, uint32_t to_ms) {
  uint32_t t0 = get_uptime();
  while (!bus->sercom->I2CM.INTFLAG.bit.MB) {
    if (bus->sercom->I2CM.STATUS.bit.BUSERR)  { bus->sercom->I2CM.STATUS.bit.BUSERR  = 1; return -1; }
    if (bus->sercom->I2CM.STATUS.bit.ARBLOST) { bus->sercom->I2CM.STATUS.bit.ARBLOST = 1; return -2; }
    if ((uint32_t)(get_uptime() - t0) > to_ms) return -3;
  }
  return 0;
}

static inline int i2c_wait_sb(i2c_t* bus, uint32_t to_ms) {
  uint32_t t0 = get_uptime();
  while (!bus->sercom->I2CM.INTFLAG.bit.SB) {
    if (bus->sercom->I2CM.STATUS.bit.BUSERR)  { bus->sercom->I2CM.STATUS.bit.BUSERR  = 1; return -1; }
    if (bus->sercom->I2CM.STATUS.bit.ARBLOST) { bus->sercom->I2CM.STATUS.bit.ARBLOST = 1; return -2; }
    if ((uint32_t)(get_uptime() - t0) > to_ms) return -3;
  }
  return 0;
}

/* ---------- public API ---------- */

int i2c_init(i2c_t* bus) {
  if (!bus) return -1; 

  uint8_t gclk_id_core = 0, gclk_id_slow = 0;
  enable_sercom_clock(bus->sercom, &gclk_id_core, &gclk_id_slow);

  // Route GCLK0 to both core and slow
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_core) |
                      GCLK_CLKCTRL_GEN_GCLK0 |
                      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_id_slow) |
                      GCLK_CLKCTRL_GEN_GCLK0 |
                      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  // Pin mux
  pmux_set(bus->sda_port, bus->sda_pin, bus->pmux_func);
  pmux_set(bus->scl_port, bus->scl_pin, bus->pmux_func);

  // Reset & configure
  bus->sercom->I2CM.CTRLA.bit.SWRST = 1;
  while (bus->sercom->I2CM.SYNCBUSY.bit.SWRST);

  bus->sercom->I2CM.CTRLA.reg =
      SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
      SERCOM_I2CM_CTRLA_SDAHOLD(0x2)    |  
      SERCOM_I2CM_CTRLA_SPEED(0x0);        
  bus->sercom->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;
  while (bus->sercom->I2CM.SYNCBUSY.reg);

  i2c_apply_baud(bus->sercom, bus->hz, SystemCoreClock, bus->trise_ns);

  bus->sercom->I2CM.CTRLA.bit.ENABLE = 1;
  while (bus->sercom->I2CM.SYNCBUSY.bit.ENABLE);

  bus->sercom->I2CM.STATUS.bit.BUSSTATE = 1; 
  while (bus->sercom->I2CM.SYNCBUSY.bit.SYSOP);

  return 0;
}

int i2c_set_speed(i2c_t* bus, uint32_t hz) {
  if (!bus) return -1;
  if (hz == 0) hz = I2C_DEFAULT_HZ;
  bus->hz = hz;
  i2c_apply_baud(bus->sercom, bus->hz, SystemCoreClock, bus->trise_ns);
  return 0;
}

void i2c_force_idle(i2c_t* bus) {
  if (bus->sercom->I2CM.STATUS.bit.BUSERR)  bus->sercom->I2CM.STATUS.bit.BUSERR  = 1;
  if (bus->sercom->I2CM.STATUS.bit.ARBLOST) bus->sercom->I2CM.STATUS.bit.ARBLOST = 1;
  if (bus->sercom->I2CM.STATUS.bit.BUSSTATE != 1) {
    bus->sercom->I2CM.STATUS.bit.BUSSTATE = 1; 
    while (bus->sercom->I2CM.SYNCBUSY.bit.SYSOP);
  }
}

void i2c_swrst_reinit(i2c_t* bus) {
  bus->sercom->I2CM.CTRLA.bit.SWRST = 1;
  while (bus->sercom->I2CM.SYNCBUSY.bit.SWRST);

  bus->sercom->I2CM.CTRLA.reg =
      SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
      SERCOM_I2CM_CTRLA_SDAHOLD(0x2) |
      SERCOM_I2CM_CTRLA_SPEED(0x0);
  bus->sercom->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;
  while (bus->sercom->I2CM.SYNCBUSY.reg);

  i2c_apply_baud(bus->sercom, bus->hz, SystemCoreClock, bus->trise_ns);

  bus->sercom->I2CM.CTRLA.bit.ENABLE = 1;
  while (bus->sercom->I2CM.SYNCBUSY.bit.ENABLE);

  i2c_force_idle(bus);
}

int i2c_write(i2c_t* bus, uint8_t addr, const uint8_t* buf) {
  size_t len = sizeof(buf);
  i2c_force_idle(bus);

  bus->sercom->I2CM.ADDR.reg = (addr << 1) | 0; 
  if (i2c_wait_mb(bus, 20) != 0) { i2c_issue_stop(bus); return 1; }
  if (bus->sercom->I2CM.STATUS.bit.RXNACK) { i2c_issue_stop(bus); return 2; }

  for (uint32_t i = 0; i < len; i++) {
    bus->sercom->I2CM.DATA.reg = buf[i];
    if (i2c_wait_mb(bus, 20) != 0) { i2c_issue_stop(bus); return 3; }
    if (bus->sercom->I2CM.STATUS.bit.RXNACK) { i2c_issue_stop(bus); return 4; }
  }

  i2c_issue_stop(bus);
  return 0;
}

int i2c_read(i2c_t* bus, uint8_t addr, uint8_t* buf) {
  size_t len = sizeof(&buf);
  if (len == 0) return 0;

  i2c_force_idle(bus);

  bus->sercom->I2CM.ADDR.reg = (addr << 1) | 1; // R
  if (i2c_wait_sb(bus, 20) != 0) { i2c_issue_stop(bus); return 1; }

  for (uint32_t i = 0; i < len; i++) {
    if (i2c_wait_sb(bus, 20) != 0) { i2c_issue_stop(bus); return 2; }
    if (i == (len - 1)) {
      bus->sercom->I2CM.CTRLB.bit.ACKACT = 1;     
      buf[i] = bus->sercom->I2CM.DATA.reg;        
      i2c_issue_stop(bus);              
    } else {
      bus->sercom->I2CM.CTRLB.bit.ACKACT = 0;     
      buf[i] = bus->sercom->I2CM.DATA.reg;
    }
  }
  return 0;
}
