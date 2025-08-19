#include "i2c.h"

static inline PortGroup* _port(uint8_t p) { return (p == 0) ? PORT->Group : &PORT->Group[1]; }

static void _pinmux_to_sercom(uint8_t port, uint8_t pin, uint8_t pmux_func) {
    PortGroup* g = _port(port);
    // Enable peripheral mux on this pin
    if (pin & 1) {
        // odd pin uses PMUXO
        g->PMUX[pin >> 1].bit.PMUXO = pmux_func;
    } else {
        // even pin uses PMUXE
        g->PMUX[pin >> 1].bit.PMUXE = pmux_func;
    }
    g->PINCFG[pin].bit.PMUXEN = 1;
    g->PINCFG[pin].bit.INEN   = 1; // enable input buffer
    g->PINCFG[pin].bit.PULLEN = 0; // let external pulls handle it (I2C has internal)
}

// BAUD for standard/fast mode (SAMD21 datasheet, simple formula)
static uint8_t _baud_from_hz(uint32_t fref, uint32_t fscl) {
    // From datasheet: BAUD = (fref / (2*fscl)) - 5  (valid for fscl<=400k, rise time small)
    // Clamp minimally
    int32_t baud = (int32_t)(fref / (2u * fscl)) - 5;
    if (baud < 1)   baud = 1;
    if (baud > 255) baud = 255;
    return (uint8_t)baud;
}

void i2c_init(i2c_t* b, const i2c_cfg_t* cfg, uint32_t hz) {
    b->cfg = *cfg;
    b->hz  = hz;
    b->initialized = false;
    b->busy = false;
    b->last_error = 0;
    b->tx_len = 0;
    b->rx_len = 0;
    b->rx_pos = 0;

    // 1) Power Manager APBC
    PM->APBCMASK.reg |= b->cfg.pm_apbcmask_bit;

    // 2) Route generic clock to SERCOM core
    // Use GCLK0 (48 MHz) to SERCOMx core
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(b->cfg.gclk_id_core) |
                        GCLK_CLKCTRL_GEN_GCLK0 |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY) {}

    // 3) Reset SERCOM I2C
    b->cfg.sercom->I2CM.CTRLA.reg = SERCOM_I2CM_CTRLA_SWRST;
    while (b->cfg.sercom->I2CM.SYNCBUSY.bit.SWRST || b->cfg.sercom->I2CM.CTRLA.bit.SWRST) {}

    // 4) Pinmux
    _pinmux_to_sercom(b->cfg.sda_port, b->cfg.sda_pin, b->cfg.pmux_func);
    _pinmux_to_sercom(b->cfg.scl_port, b->cfg.scl_pin, b->cfg.pmux_func);

    // 5) Configure as I2C master, smart mode for easier reads, SDAHOLD default (75–450 ns)
    b->cfg.sercom->I2CM.CTRLA.reg =
        SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
        SERCOM_I2CM_CTRLA_SPEED(0) |       // 0=Standard/Fast
        SERCOM_I2CM_CTRLA_SDAHOLD(0x2) |   // 300–600 ns
        SERCOM_I2CM_CTRLA_SCLSM;           // SCL stretch after ACK

    // 6) Baud
    b->cfg.sercom->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(_baud_from_hz(I2C_FREF_HZ, b->hz));

    // 7) Enable + Bus state to IDLE
    b->cfg.sercom->I2CM.CTRLB.reg = SERCOM_I2CM_CTRLB_SMEN; // smart mode
    b->cfg.sercom->I2CM.CTRLA.bit.ENABLE = 1;
    while (b->cfg.sercom->I2CM.SYNCBUSY.bit.ENABLE) {}

    b->cfg.sercom->I2CM.STATUS.reg = SERCOM_I2CM_STATUS_BUSSTATE(0x1); // IDLE
    while (b->cfg.sercom->I2CM.SYNCBUSY.bit.SYSOP) {}

    b->initialized = true;
}

void i2c_set_clock(i2c_t* b, uint32_t hz) {
    b->hz = hz;
    b->cfg.sercom->I2CM.CTRLA.bit.ENABLE = 0;
    while (b->cfg.sercom->I2CM.SYNCBUSY.bit.ENABLE) {}
    b->cfg.sercom->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(_baud_from_hz(I2C_FREF_HZ, b->hz));
    b->cfg.sercom->I2CM.CTRLA.bit.ENABLE = 1;
    while (b->cfg.sercom->I2CM.SYNCBUSY.bit.ENABLE) {}
}

// --- tiny helpers ---
static inline void _cmd(i2c_t* b, uint8_t cmd) {
    b->cfg.sercom->I2CM.CTRLB.bit.CMD = cmd;
    while (b->cfg.sercom->I2CM.SYNCBUSY.bit.SYSOP) {}
}
static inline void _send_addr(i2c_t* b, uint8_t addr_rw) {
    b->cfg.sercom->I2CM.ADDR.reg = addr_rw;
    // wait for MB (master on bus) / SB flags
    while (!(b->cfg.sercom->I2CM.INTFLAG.reg & (SERCOM_I2CM_INTFLAG_MB | SERCOM_I2CM_INTFLAG_SB))) {}
}
static inline bool _nack_or_buserr(i2c_t* b) {
    uint16_t st = b->cfg.sercom->I2CM.STATUS.reg;
    return (st & (SERCOM_I2CM_STATUS_RXNACK | SERCOM_I2CM_STATUS_BUSERR));
}

// --- Arduino-like API, but per-instance ---
void i2c_begin_tx(i2c_t* b, uint8_t addr7) {
    b->tx_len = 0;
    b->busy = true;
    // START + write address
    _send_addr(b, (uint8_t)((addr7 << 1) | 0));
}

size_t i2c_write(i2c_t* b, const uint8_t* data, size_t len) {
    size_t n = 0;
    while (n < len && b->tx_len < I2C_TX_BUF_SZ) {
        b->tx[b->tx_len++] = data[n++];
    }
    return n;
}

size_t i2c_write_byte(i2c_t* b, uint8_t byte) {
    return i2c_write(b, &byte, 1);
}

uint8_t i2c_end_tx(i2c_t* b, int sendStop) {
    // push TX bytes
    for (size_t i = 0; i < b->tx_len; ++i) {
        b->cfg.sercom->I2CM.DATA.reg = b->tx[i];
        while (!(b->cfg.sercom->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_MB)) {}
        if (_nack_or_buserr(b)) { b->last_error = 1; goto abort; }
    }
    b->last_error = 0;
abort:
    if (sendStop) _cmd(b, 0x3); // STOP
    b->busy = false;
    return b->last_error;
}

size_t i2c_request_from(i2c_t* b, uint8_t addr7, size_t len, int sendStop) {
    if (len > I2C_RX_BUF_SZ) len = I2C_RX_BUF_SZ;
    b->rx_len = 0;
    b->rx_pos = 0;

    // repeated START if already busy
    _send_addr(b, (uint8_t)((addr7 << 1) | 1));
    if (_nack_or_buserr(b)) { b->last_error = 2; goto out; }

    // Read N bytes: send ACK for all but last, then NACK+STOP if sendStop
    for (size_t i = 0; i < len; ++i) {
        if (i + 1 == len) {
            // last byte -> prepare NACK
            b->cfg.sercom->I2CM.CTRLB.bit.ACKACT = 1;
        } else {
            b->cfg.sercom->I2CM.CTRLB.bit.ACKACT = 0;
        }
        _cmd(b, 0x2); // CMD=2 -> read action (master read)
        while (!(b->cfg.sercom->I2CM.INTFLAG.reg & SERCOM_I2CM_INTFLAG_SB)) {}
        b->rx[b->rx_len++] = b->cfg.sercom->I2CM.DATA.reg;
    }

    if (sendStop) _cmd(b, 0x3); // STOP
    b->last_error = 0;

out:
    b->busy = false;
    return b->rx_len;
}

int i2c_read(i2c_t* b) {
    if (b->rx_pos >= b->rx_len) return -1;
    return (int)b->rx[b->rx_pos++];
}

size_t i2c_available(const i2c_t* b) {
    return (b->rx_len - b->rx_pos);
}
