#include "i2c-master.h"
#include "i2c-helper.h"
#include "samd21.h"
#include "system_samd21.h"
#include "variables.h"
#include "clock.h"    
#include <stdint.h>

uint8_t i2c_m_init(i2c_t* bus) {
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

uint8_t i2c_m_set_speed(i2c_t* bus, uint32_t hz) {
  if (!bus) return -1;
  if (hz == 0) hz = I2C_DEFAULT_HZ;
  bus->hz = hz;
  i2c_apply_baud(bus->sercom, bus->hz, SystemCoreClock, bus->trise_ns);
  return 0;
}

void i2c_m_force_idle(i2c_t* bus) {
  if (bus->sercom->I2CM.STATUS.bit.BUSERR)  bus->sercom->I2CM.STATUS.bit.BUSERR  = 1;
  if (bus->sercom->I2CM.STATUS.bit.ARBLOST) bus->sercom->I2CM.STATUS.bit.ARBLOST = 1;
  if (bus->sercom->I2CM.STATUS.bit.BUSSTATE != 1) {
    bus->sercom->I2CM.STATUS.bit.BUSSTATE = 1; 
    while (bus->sercom->I2CM.SYNCBUSY.bit.SYSOP);
  }
}

void i2c_m_swrst_reinit(i2c_t* bus) {
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

  i2c_m_force_idle(bus);
}

uint8_t i2c_m_write(i2c_t* bus, uint8_t addr, const uint8_t* buf, size_t len) {
    if (!bus || !buf || len == 0) return 0xFF;
    i2c_m_force_idle(bus);

    bus->sercom->I2CM.ADDR.reg = (addr << 1) | 0; // write
    if (i2c_wait_mb(bus, 20) != 0) { i2c_issue_stop(bus); return 1; }
    if (bus->sercom->I2CM.STATUS.bit.RXNACK) { i2c_issue_stop(bus); return 2; }

    for (size_t i = 0; i < len; i++) {
        bus->sercom->I2CM.DATA.reg = buf[i];
        if (i2c_wait_mb(bus, 20) != 0) { i2c_issue_stop(bus); return 3; }
        if (bus->sercom->I2CM.STATUS.bit.RXNACK) { i2c_issue_stop(bus); return 4; }
    }

    i2c_issue_stop(bus);
    return 0;
}

uint8_t i2c_m_read(i2c_t* bus, uint8_t addr, uint8_t* buf, size_t len) {
    if (!bus || !buf || len == 0) return 0xFF;
    i2c_m_force_idle(bus);

    bus->sercom->I2CM.ADDR.reg = (addr << 1) | 1; // read
    if (i2c_wait_sb(bus, 20) != 0) { i2c_issue_stop(bus); return 1; }

    for (size_t i = 0; i < len; i++) {
        if (i2c_wait_sb(bus, 20) != 0) { i2c_issue_stop(bus); return 2; }

        if (i == len - 1) {
            bus->sercom->I2CM.CTRLB.bit.ACKACT = 1; // NACK last byte
            buf[i] = bus->sercom->I2CM.DATA.reg;
            i2c_issue_stop(bus);
        } else {
            bus->sercom->I2CM.CTRLB.bit.ACKACT = 0; // ACK
            buf[i] = bus->sercom->I2CM.DATA.reg;
        }
    }
    return 0;
}
