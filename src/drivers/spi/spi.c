#include "spi.h"

inline void _spi_clear_errors(SercomSpi *spi) {
  if (spi->STATUS.reg & SERCOM_SPI_STATUS_BUFOVF) {
    spi->STATUS.reg = SERCOM_SPI_STATUS_BUFOVF;  // write-1-to-clear
  }
}

inline void _sercom_sync(SercomSpi *s) {
  // For SERCOM SPI on SAMD21, SYNCBUSY is its own register
  while (s->SYNCBUSY.reg) { __NOP(); }
}

void spi_enable_clock(uint8_t gclk_id_core, uint32_t apbc_mask_bit, uint8_t gclk_src_id)
{
  // Enable APBC clock for the SERCOM instance
  PM->APBCMASK.reg |= apbc_mask_bit;

  // Route GCLK generator to SERCOMx core
  GCLK->CLKCTRL.reg =
      GCLK_CLKCTRL_ID(gclk_id_core) |
      GCLK_CLKCTRL_GEN(gclk_src_id) |
      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY) { /* wait */ }
}

void spi_init(spi_t *bus, SercomSpi *spi, uint32_t gclk_freq_hz,
              uint32_t sck_hz, bool cpol, bool cpha,
              uint8_t dipo, uint8_t dopo, bool msb_first)
{
  bus->spi = spi;
  bus->gclk_freq_hz = gclk_freq_hz;

  // Disable before config
  spi->CTRLA.bit.ENABLE = 0;
  _sercom_sync(spi);

  // Reset
  spi->CTRLA.bit.SWRST = 1;
  while (spi->CTRLA.bit.SWRST || spi->SYNCBUSY.bit.SWRST) { /* wait */ }

  // Master mode, CPOL/CPHA, DOPO/DIPO, MSB/LSB
  spi->CTRLA.reg =
      SERCOM_SPI_CTRLA_MODE(0x3) |         // SPI Master
      (cpol ? SERCOM_SPI_CTRLA_CPOL : 0) |
      (cpha ? SERCOM_SPI_CTRLA_CPHA : 0) |
      SERCOM_SPI_CTRLA_DIPO(dipo & 0x3) |
      SERCOM_SPI_CTRLA_DOPO(dopo & 0x3) |
      (msb_first ? 0 : SERCOM_SPI_CTRLA_DORD);

  // Enable receiver
  spi->CTRLB.reg = SERCOM_SPI_CTRLB_RXEN;
  _sercom_sync(spi);

  // BAUD = fref/(2*fsck) - 1  (8-bit)
  uint32_t baud = 0;
  if (sck_hz == 0) sck_hz = 1000000;
  if (sck_hz >= (gclk_freq_hz / 2u)) baud = 0;
  else {
    uint32_t t = (gclk_freq_hz / (2u * sck_hz));
    if (t == 0) t = 1;
    if (t > 256u) t = 256u;
    baud = t - 1u;
  }
  spi->BAUD.reg = (uint8_t)baud;

  // Enable
  spi->CTRLA.bit.ENABLE = 1;
  _sercom_sync(spi);
}

void spi_set_baud(spi_t *bus, uint32_t sck_hz)
{
  SercomSpi *spi = bus->spi;
  uint32_t fref = bus->gclk_freq_hz;

  uint32_t baud = 0;
  if (sck_hz >= (fref / 2u)) baud = 0;
  else {
    uint32_t t = (fref / (2u * sck_hz));
    if (t == 0) t = 1;
    if (t > 256u) t = 256u;
    baud = t - 1u;
  }
  spi->BAUD.reg = (uint8_t)baud;
  _sercom_sync(spi);
}

uint8_t spi_txrx(spi_t *bus, uint8_t byte)
{
  SercomSpi *spi = bus->spi;

  // Wait for Data Register Empty
  while (!spi->INTFLAG.bit.DRE) { /* wait */ }

  spi->DATA.reg = byte;

  // Wait for Receive Complete
  while (!spi->INTFLAG.bit.RXC) { /* wait */ }

  uint8_t r = (uint8_t)spi->DATA.reg;
  return r;
}

void spi_transfer(spi_t *bus, const uint8_t *tx, uint8_t *rx, size_t len)
{
  SercomSpi *spi = bus->spi;
  for (size_t i = 0; i < len; i++) {
    uint8_t out = tx ? tx[i] : 0xFF;
    while (!spi->INTFLAG.bit.DRE) { /* wait */ }
    spi->DATA.reg = out;
    while (!spi->INTFLAG.bit.RXC) { /* wait */ }
    uint8_t in = (uint8_t)spi->DATA.reg;
    if (rx) rx[i] = in;
  }
  // Ensure last byte is fully shifted
  while (!spi->INTFLAG.bit.TXC) { /* wait */ }
}

void spi_wait_idle(spi_t *bus)
{
  // Wait until not busy and transmit complete
  while (bus->spi->INTFLAG.bit.TXC == 0) { /* wait */ }
}

