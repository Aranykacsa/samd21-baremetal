#include "i2c-slave.h"
#include "i2c-helper.h"
#include "samd21.h"
#include "system_samd21.h"
#include "variables.h"
#include "clock.h"     // must provide get_uptime() in ms
#include <stdint.h>
#include <stddef.h>

/* ------ SLAVE ------ */

static inline void _i2cs_clear_all(Sercom* s) {
  /* Clear pending INTFLAGs (write-1-to-clear) */
  s->I2CS.INTFLAG.reg = s->I2CS.INTFLAG.reg;
  /* Clear sticky STATUS errors */
  if (s->I2CS.STATUS.bit.BUSERR) s->I2CS.STATUS.bit.BUSERR = 1;
  if (s->I2CS.STATUS.bit.COLL)   s->I2CS.STATUS.bit.COLL   = 1;
}

void i2c_s_force_idle(i2c_t* bus) {
  Sercom* s = bus->sercom;
  if (s->I2CS.STATUS.bit.BUSERR) s->I2CS.STATUS.bit.BUSERR = 1;
  if (s->I2CS.STATUS.bit.COLL)   s->I2CS.STATUS.bit.COLL   = 1;
  _i2cs_clear_all(s);
}

uint8_t i2c_s_init(i2c_t* bus, uint8_t addr, uint8_t addr_mask) {
  if (!bus || !bus->sercom) return -1;

  uint8_t gclk_core = 0, gclk_slow = 0;
  enable_sercom_clock(bus->sercom, &gclk_core, &gclk_slow);

  /* Route GCLK0 to core/slow to match your style */
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_core) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(gclk_slow) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  /* Pin muxing */
  pmux_set(bus->sda_port, bus->sda_pin, bus->pmux_func);
  pmux_set(bus->scl_port, bus->scl_pin, bus->pmux_func);

  /* Software reset */
  bus->sercom->I2CS.CTRLA.bit.SWRST = 1;
  while (bus->sercom->I2CS.SYNCBUSY.bit.SWRST);

  /* Configure SLAVE mode */
  bus->sercom->I2CS.CTRLA.reg =
      SERCOM_I2CS_CTRLA_MODE_I2C_SLAVE |
      SERCOM_I2CS_CTRLA_SDAHOLD(0x2) |   /* robust hold ~300–600 ns */
      SERCOM_I2CS_CTRLA_SCLSM;           /* allow SCL stretch on match if needed */

  /* Smart mode: auto ACK/NACK via CTRLB.ACKACT + CMD */
  bus->sercom->I2CS.CTRLB.reg = SERCOM_I2CS_CTRLB_SMEN;
  while (bus->sercom->I2CS.SYNCBUSY.reg);

  /* Set 7-bit address and mask (1 bits in mask = don't care) */
  bus->sercom->I2CS.ADDR.reg =
      SERCOM_I2CS_ADDR_ADDR(addr) |
      SERCOM_I2CS_ADDR_ADDRMASK(addr_mask);

  _i2cs_clear_all(bus->sercom);

  /* Enable peripheral */
  bus->sercom->I2CS.CTRLA.bit.ENABLE = 1;
  while (bus->sercom->I2CS.SYNCBUSY.bit.ENABLE);

  return 0;
}

void i2c_s_enable(i2c_t* bus) {
  bus->sercom->I2CS.CTRLA.bit.ENABLE = 1;
  while (bus->sercom->I2CS.SYNCBUSY.bit.ENABLE);
}

void i2c_s_disable(i2c_t* bus) {
  bus->sercom->I2CS.CTRLA.bit.ENABLE = 0;
  while (bus->sercom->I2CS.SYNCBUSY.bit.ENABLE);
}

uint8_t i2c_s_swrst_reinit(i2c_t* bus, uint8_t addr, uint8_t addr_mask) {
  bus->sercom->I2CS.CTRLA.bit.SWRST = 1;
  while (bus->sercom->I2CS.SYNCBUSY.bit.SWRST);
  return i2c_s_init(bus, addr, addr_mask);
}

/* Wait for address match; sets *is_read if master wants to read (slave->tx).
   Returns 0=OK, -1=timeout, -2=buserr/coll. */
uint8_t i2c_s_wait_address(i2c_t* bus, uint32_t timeout_ms, uint8_t* is_read) {
  uint32_t t0 = get_uptime();

  _i2cs_clear_all(bus->sercom);

  while (!bus->sercom->I2CS.INTFLAG.bit.AMATCH) {
    if ((uint32_t)(get_uptime() - t0) > timeout_ms) return -1;
    if (bus->sercom->I2CS.STATUS.bit.BUSERR) { bus->sercom->I2CS.STATUS.bit.BUSERR = 1; return -2; }
    if (bus->sercom->I2CS.STATUS.bit.COLL)   { bus->sercom->I2CS.STATUS.bit.COLL   = 1; return -2; }
  }

  if (is_read) *is_read = bus->sercom->I2CS.STATUS.bit.DIR ? 1 : 0;

  /* ACK the address (ACKACT=0, CMD=2) and clear AMATCH */
  bus->sercom->I2CS.CTRLB.bit.ACKACT = 0;
  bus->sercom->I2CS.CTRLB.bit.CMD    = 2;           /* send ACK */
  bus->sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_AMATCH;

  return 0;
}

/* Receive bytes (master writes to us).
   Returns number of bytes received (>=0), or <0 on error. */
uint8_t i2c_s_read(i2c_t* bus, uint8_t* buffer, size_t len, uint32_t timeout_ms) {
  if (!buffer || len == 0) return 0;
  size_t n = 0;

  while (n < len) {
    uint32_t t0 = get_uptime();
    while (!bus->sercom->I2CS.INTFLAG.bit.DRDY && !bus->sercom->I2CS.INTFLAG.bit.PREC) {
      if ((uint32_t)(get_uptime() - t0) > timeout_ms) return (int)n;  /* partial OK */
      if (bus->sercom->I2CS.STATUS.bit.BUSERR) { bus->sercom->I2CS.STATUS.bit.BUSERR = 1; return n ? (int)n : -2; }
      if (bus->sercom->I2CS.STATUS.bit.COLL)   { bus->sercom->I2CS.STATUS.bit.COLL   = 1; return n ? (int)n : -2; }
    }

    if (bus->sercom->I2CS.INTFLAG.bit.PREC) {      /* STOP */
      bus->sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_PREC;
      break;
    }

    buffer[n++] = bus->sercom->I2CS.DATA.reg;
    bus->sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_DRDY;

    /* ACK next if we still have room; else NACK to signal we're full */
    bus->sercom->I2CS.CTRLB.bit.ACKACT = (n < len) ? 0 : 1;
    bus->sercom->I2CS.CTRLB.bit.CMD    = 2;        /* send ACK/NACK */
  }

  return (int)n;
}

/* Transmit bytes (master reads from us).
   Returns number of bytes sent (>=0), or <0 on error. */
uint8_t i2c_s_write(i2c_t* bus, const uint8_t* buffer, size_t len, uint32_t timeout_ms) {
  if (!buffer || len == 0) return 0;
  size_t n = 0;

  while (n < len) {
    bus->sercom->I2CS.DATA.reg = buffer[n];

    uint32_t t0 = get_uptime();
    while (!bus->sercom->I2CS.INTFLAG.bit.DRDY && !bus->sercom->I2CS.INTFLAG.bit.PREC) {
      if ((uint32_t)(get_uptime() - t0) > timeout_ms) return (int)n;  /* partial OK */
      if (bus->sercom->I2CS.STATUS.bit.BUSERR) { bus->sercom->I2CS.STATUS.bit.BUSERR = 1; return n ? (int)n : -2; }
      if (bus->sercom->I2CS.STATUS.bit.COLL)   { bus->sercom->I2CS.STATUS.bit.COLL   = 1; return n ? (int)n : -2; }
    }

    if (bus->sercom->I2CS.INTFLAG.bit.PREC) {      /* STOP */
      bus->sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_PREC;
      break;
    }

    /* Byte phase done */
    bus->sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_DRDY;

    if (bus->sercom->I2CS.STATUS.bit.RXNACK) {
      /* Master NACKed last byte -> stop sending */
      bus->sercom->I2CS.CTRLB.bit.ACKACT = 1;
      bus->sercom->I2CS.CTRLB.bit.CMD    = 2;
      break;
    } else {
      n++;
      bus->sercom->I2CS.CTRLB.bit.ACKACT = 0;
      bus->sercom->I2CS.CTRLB.bit.CMD    = 2;
    }
  }

  return (int)n;
}

uint8_t i2c_s_wait_stop(i2c_t* bus, uint32_t timeout_ms) {
  uint32_t t0 = get_uptime();

  while (!bus->sercom->I2CS.INTFLAG.bit.PREC) {
    if ((uint32_t)(get_uptime() - t0) > timeout_ms) return -1;
    if (bus->sercom->I2CS.STATUS.bit.BUSERR) { bus->sercom->I2CS.STATUS.bit.BUSERR = 1; return -2; }
    if (bus->sercom->I2CS.STATUS.bit.COLL)   { bus->sercom->I2CS.STATUS.bit.COLL   = 1; return -2; }
  }
  bus->sercom->I2CS.INTFLAG.reg = SERCOM_I2CS_INTFLAG_PREC;
  return 0;
}