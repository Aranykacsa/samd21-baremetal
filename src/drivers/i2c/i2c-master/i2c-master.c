#include "i2c-master.h"

static inline void i2c_pinmux(uint8_t port, uint8_t pin, uint8_t func) {
    PORT->Group[port].PINCFG[pin].bit.PMUXEN = 1;
    if (pin & 1)
        PORT->Group[port].PMUX[pin/2].bit.PMUXO = func;
    else
        PORT->Group[port].PMUX[pin/2].bit.PMUXE = func;
}

static inline void i2c_enable_gclk(Sercom* sercom) {
    uint32_t id_core = 0;
    uint32_t apbc_bit = 0;

    if (sercom == SERCOM0) { id_core = SERCOM0_GCLK_ID_CORE; apbc_bit = PM_APBCMASK_SERCOM0; }
    if (sercom == SERCOM1) { id_core = SERCOM1_GCLK_ID_CORE; apbc_bit = PM_APBCMASK_SERCOM1; }
    if (sercom == SERCOM2) { id_core = SERCOM2_GCLK_ID_CORE; apbc_bit = PM_APBCMASK_SERCOM2; }
    if (sercom == SERCOM3) { id_core = SERCOM3_GCLK_ID_CORE; apbc_bit = PM_APBCMASK_SERCOM3; }
    //if (sercom == SERCOM4) { id_core = SERCOM4_GCLK_ID_CORE; apbc_bit = PM_APBCMASK_SERCOM4; }
    //if (sercom == SERCOM5) { id_core = SERCOM5_GCLK_ID_CORE; apbc_bit = PM_APBCMASK_SERCOM5; }

    PM->APBCMASK.reg |= apbc_bit;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(id_core) |
                        GCLK_CLKCTRL_GEN(0) |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);
}

static inline uint32_t i2c_baud_calc(uint32_t f_gclk, uint32_t f_scl, uint32_t trise_ns) {
    uint32_t tr = (uint64_t)f_gclk * trise_ns / 1000000000ULL;
    uint32_t baud = (f_gclk / (2 * f_scl)) - 5 - tr;
    if ((int32_t)baud < 1) baud = 1;
    return baud;
}

void i2c_master_init(i2c_t* bus, uint32_t f_gclk) {
    i2c_enable_gclk(bus->sercom);

    i2c_pinmux(bus->sda_port, bus->sda_pin, bus->pmux_func);
    i2c_pinmux(bus->scl_port, bus->scl_pin, bus->pmux_func);

    SercomI2cm* i2cm = &bus->sercom->I2CM;

    i2cm->CTRLA.bit.SWRST = 1;
    while (i2cm->SYNCBUSY.bit.SWRST);

    i2cm->CTRLA.reg = SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
                      SERCOM_I2CM_CTRLA_SPEED(0) |           // Standard/Fast
                      SERCOM_I2CM_CTRLA_SDAHOLD(2);          // 300–600 ns hold

    i2cm->CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN;                // smart mode

    uint32_t baud = i2c_baud_calc(f_gclk, bus->hz, bus->trise_ns);
    i2cm->BAUD.reg = SERCOM_I2CM_BAUD_BAUD(baud) | SERCOM_I2CM_BAUD_BAUDLOW(baud);

    i2cm->CTRLA.bit.ENABLE = 1;
    while (i2cm->SYNCBUSY.bit.ENABLE);

    i2cm->STATUS.bit.BUSSTATE = 1; // force IDLE
    while (i2cm->SYNCBUSY.bit.SYSOP);
}

bool i2c_master_write(i2c_t* bus, uint8_t addr, const uint8_t* data, uint16_t len) {
    SercomI2cm* i2cm = &bus->sercom->I2CM;

    i2cm->ADDR.reg = (addr << 1) | 0; // write
    while (!i2cm->INTFLAG.bit.MB && !i2cm->INTFLAG.bit.SB);

    if (i2cm->STATUS.bit.RXNACK) return false;

    for (uint16_t i = 0; i < len; i++) {
        i2cm->DATA.reg = data[i];
        while (!i2cm->INTFLAG.bit.MB);
        if (i2cm->STATUS.bit.RXNACK) return false;
    }

    i2cm->CTRLB.bit.CMD = 3; // STOP
    while (i2cm->SYNCBUSY.bit.SYSOP);
    return true;
}

bool i2c_master_read(i2c_t* bus, uint8_t addr, uint8_t* data, uint16_t len) {
    SercomI2cm* i2cm = &bus->sercom->I2CM;

    i2cm->ADDR.reg = (addr << 1) | 1; // read
    while (!i2cm->INTFLAG.bit.SB);
    if (i2cm->STATUS.bit.RXNACK) return false;

    for (uint16_t i = 0; i < len; i++) {
        while (!i2cm->INTFLAG.bit.SB);
        data[i] = i2cm->DATA.reg;
        if (i == len - 1)
            i2cm->CTRLB.reg = SERCOM_I2CM_CTRLB_ACKACT | SERCOM_I2CM_CTRLB_CMD(3); // NACK+STOP
        else
            i2cm->CTRLB.reg = SERCOM_I2CM_CTRLB_CMD(3); // ACK next
        while (i2cm->SYNCBUSY.bit.SYSOP);
    }

    return true;
}
