#include "i2c-slave.h"

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

void i2c_slave_init(i2c_t* bus, uint8_t addr) {
    i2c_enable_gclk(bus->sercom);
    i2c_pinmux(bus->sda_port, bus->sda_pin, bus->pmux_func);
    i2c_pinmux(bus->scl_port, bus->scl_pin, bus->pmux_func);

    SercomI2cs* i2cs = &bus->sercom->I2CS;

    i2cs->CTRLA.bit.SWRST = 1;
    while (i2cs->SYNCBUSY.bit.SWRST);

    i2cs->CTRLA.reg = SERCOM_I2CS_CTRLA_MODE_I2C_SLAVE |
                      SERCOM_I2CS_CTRLA_SDAHOLD(2);

    i2cs->CTRLB.reg = SERCOM_I2CS_CTRLB_SMEN;

    i2cs->ADDR.reg = SERCOM_I2CS_ADDR_ADDR(addr << 1);

    i2cs->INTENSET.reg = SERCOM_I2CS_INTENSET_AMATCH |
                         SERCOM_I2CS_INTENSET_DRDY |
                         SERCOM_I2CS_INTENSET_PREC;

    i2cs->CTRLA.bit.ENABLE = 1;
    while (i2cs->SYNCBUSY.bit.ENABLE);
}

void i2c_slave_task(i2c_t* bus) {
    SercomI2cs* i2cs = &bus->sercom->I2CS;
    uint32_t flags = i2cs->INTFLAG.reg;

    if (flags & SERCOM_I2CS_INTFLAG_AMATCH) {
        (void)i2cs->DATA.reg;
        i2cs->INTFLAG.reg = SERCOM_I2CS_INTFLAG_AMATCH;
    }

    if (flags & SERCOM_I2CS_INTFLAG_DRDY) {
        if (i2cs->STATUS.bit.DIR) {
            // master reading from us
            i2cs->DATA.reg = 0x42; // example data
        } else {
            // master writing to us
            volatile uint8_t rx = i2cs->DATA.reg;
            (void)rx;
        }
        i2cs->INTFLAG.reg = SERCOM_I2CS_INTFLAG_DRDY;
    }

    if (flags & SERCOM_I2CS_INTFLAG_PREC) {
        i2cs->INTFLAG.reg = SERCOM_I2CS_INTFLAG_PREC;
    }
}
