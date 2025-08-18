// ========================= i2c_wire.c =========================
#include "i2c.h"

// ---- static state ----
static uint8_t  s_tx[I2C_TX_BUF_SZ];
static size_t   s_tx_len;
static uint8_t  s_rx[I2C_RX_BUF_SZ];
static size_t   s_rx_len;
static size_t   s_rx_idx;
static uint8_t  s_addr7;

// ---- helpers ----
static inline PortGroup* _pg(uint8_t port){ return &PORT->Group[port]; }

static void _i2c_port_init(void){
  PortGroup *pg = _pg(I2C_SDA_PORT);
  // enable input + weak pull-ups (still use external 2.2k–4.7k in real HW)
  pg->PINCFG[I2C_SDA_PIN].bit.INEN = 1;
  pg->PINCFG[I2C_SDA_PIN].bit.PULLEN = 1;
  pg->OUTSET.reg = (1u << I2C_SDA_PIN);
  pg->PINCFG[I2C_SCL_PIN].bit.INEN = 1;
  pg->PINCFG[I2C_SCL_PIN].bit.PULLEN = 1;
  pg->OUTSET.reg = (1u << I2C_SCL_PIN);
  // PMUX function
  uint8_t idx;
  idx = I2C_SDA_PIN/2u;
  if ((I2C_SDA_PIN & 1u)==0u) pg->PMUX[idx].bit.PMUXE = I2C_PMUX_FUNC; else pg->PMUX[idx].bit.PMUXO = I2C_PMUX_FUNC;
  idx = I2C_SCL_PIN/2u;
  if ((I2C_SCL_PIN & 1u)==0u) pg->PMUX[idx].bit.PMUXE = I2C_PMUX_FUNC; else pg->PMUX[idx].bit.PMUXO = I2C_PMUX_FUNC;
  pg->PINCFG[I2C_SDA_PIN].bit.PMUXEN = 1;
  pg->PINCFG[I2C_SCL_PIN].bit.PMUXEN = 1;
}

static void _i2c_clock_on(void){
  PM->APBCMASK.reg |= I2C_PM_APBCMASK_BIT;
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(I2C_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);
}

static inline uint8_t _baud_from_hz(uint32_t fref, uint32_t fscl){
  if (fscl==0) fscl = 100000u;
  int32_t baud = (int32_t)( (fref / (2u*fscl)) - 5 );
  if (baud < 0) baud = 0; if (baud > 255) baud = 255; return (uint8_t)baud;
}

static void _i2c_enable_master(uint32_t hz){
  I2C_SERCOM->I2CM.CTRLA.bit.SWRST = 1;
  while (I2C_SERCOM->I2CM.SYNCBUSY.bit.SWRST);
  I2C_SERCOM->I2CM.CTRLA.reg =
      SERCOM_I2CM_CTRLA_MODE_I2C_MASTER |
      SERCOM_I2CM_CTRLA_SCLSM |
      SERCOM_I2CM_CTRLA_SPEED(0) |
      SERCOM_I2CM_CTRLA_SDAHOLD(3);
  I2C_SERCOM->I2CM.BAUD.reg = SERCOM_I2CM_BAUD_BAUD(_baud_from_hz(I2C_FREF_HZ, hz));
  I2C_SERCOM->I2CM.CTRLA.bit.ENABLE = 1;
  while (I2C_SERCOM->I2CM.SYNCBUSY.bit.ENABLE);
  I2C_SERCOM->I2CM.STATUS.bit.BUSSTATE = 1; // IDLE
  while (I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
}

static inline int _wait_mb(void){
  while (!I2C_SERCOM->I2CM.INTFLAG.bit.MB){
    if (I2C_SERCOM->I2CM.STATUS.bit.BUSERR) return -1;
    if (I2C_SERCOM->I2CM.STATUS.bit.RXNACK) return -2;
  }
  return 0;
}

static inline int _wait_sb(void){
  while (!I2C_SERCOM->I2CM.INTFLAG.bit.SB){
    if (I2C_SERCOM->I2CM.STATUS.bit.BUSERR) return -1;
  }
  return 0;
}

static inline void _cmd_stop(void){
  I2C_SERCOM->I2CM.CTRLB.bit.CMD = 3; // STOP
  while (I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
}

// ---- public API ----
void i2c_begin(uint32_t hz){
  s_tx_len = s_rx_len = s_rx_idx = 0; s_addr7 = 0;
  _i2c_port_init();
  _i2c_clock_on();
  _i2c_enable_master(hz ? hz : I2C_DEFAULT_HZ);
}

void i2c_set_clock(uint32_t hz){
  _i2c_enable_master(hz ? hz : I2C_DEFAULT_HZ);
}

void i2c_begin_tx(uint8_t addr7){
  s_addr7 = addr7 & 0x7F;
  s_tx_len = 0;
}

size_t i2c_write(const uint8_t *data, size_t len){
  size_t n = 0; if (!data) return 0;
  while (n < len && s_tx_len < I2C_TX_BUF_SZ){ s_tx[s_tx_len++] = data[n++]; }
  return n;
}

size_t i2c_write_byte(uint8_t b){
  if (s_tx_len >= I2C_TX_BUF_SZ) return 0; s_tx[s_tx_len++] = b; return 1;
}

uint8_t i2c_end_tx(int sendStop){
  // Address + W
  I2C_SERCOM->I2CM.ADDR.reg = ((uint32_t)s_addr7 << 1) | 0u;
  if (_wait_mb() < 0){ _cmd_stop(); return 2; }
  // Data bytes
  for (size_t i = 0; i < s_tx_len; i++){
    I2C_SERCOM->I2CM.DATA.reg = s_tx[i];
    if (_wait_mb() < 0){ _cmd_stop(); return 3; }
  }
  if (sendStop){ _cmd_stop(); }
  s_tx_len = 0; return 0;
}

size_t i2c_request_from(uint8_t addr7, size_t len, int sendStop){
  s_rx_len = 0; s_rx_idx = 0; if (len > I2C_RX_BUF_SZ) len = I2C_RX_BUF_SZ;
  // Address + R
  I2C_SERCOM->I2CM.ADDR.reg = ((uint32_t)(addr7 & 0x7F) << 1) | 1u;
  if (_wait_sb() < 0){ _cmd_stop(); return 0; }
  for (size_t i = 0; i < len; i++){
    if (i < (len-1)){
      I2C_SERCOM->I2CM.CTRLB.bit.ACKACT = 0; // ACK
      I2C_SERCOM->I2CM.CTRLB.bit.CMD = 2;    // read next
      while (I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
      if (_wait_sb() < 0){ _cmd_stop(); return i; }
      s_rx[s_rx_len++] = (uint8_t)I2C_SERCOM->I2CM.DATA.reg;
    } else {
      I2C_SERCOM->I2CM.CTRLB.bit.ACKACT = 1; // NACK last
      if (sendStop){ I2C_SERCOM->I2CM.CTRLB.bit.CMD = 3; } else { I2C_SERCOM->I2CM.CTRLB.bit.CMD = 2; }
      while (I2C_SERCOM->I2CM.SYNCBUSY.bit.SYSOP);
      s_rx[s_rx_len++] = (uint8_t)I2C_SERCOM->I2CM.DATA.reg;
    }
  }
  return s_rx_len;
}

int i2c_read(void){ if (s_rx_idx >= s_rx_len) return -1; return (int)s_rx[s_rx_idx++]; }
size_t i2c_available(void){ return (s_rx_len - s_rx_idx); }
