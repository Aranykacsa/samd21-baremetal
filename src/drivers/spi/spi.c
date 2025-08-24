#include "spi.h"
#include "variables.h"
#include "samd21.h"
#include "clock.h"

void SD_CS_HIGH(spi_t* bus) {
  PORT->Group[(bus)->cs_port].OUTSET.reg = (1u << (bus)->cs_pin);
} 
void SD_CS_LOW(spi_t* bus)  {
  PORT->Group[(bus)->cs_port].OUTCLR.reg = (1u << (bus)->cs_pin);
}

void spi_wait_sync(spi_t* bus) {
  while (bus->sercom->SPI.SYNCBUSY.reg) {}
}

uint8_t spi_txrx(spi_t* bus, uint8_t v) {
  while (!bus->sercom->SPI.INTFLAG.bit.DRE) {}
  bus->sercom->SPI.DATA.reg = v;
  while (!bus->sercom->SPI.INTFLAG.bit.RXC) {}
  return (uint8_t)bus->sercom->SPI.DATA.reg;
}

void spi_chip_select_init(spi_t* bus) {
  PM->APBBMASK.bit.PORT_ = 1;
  PORT->Group[bus->cs_port].DIRSET.reg = (1u << bus->cs_pin);
  SD_CS_HIGH(bus);
}

void spi_pins_init(spi_t* bus) {
  PM->APBBMASK.bit.PORT_ = 1;

  // MOSI: even index -> PMUXE
  PORT->Group[bus->mosi_port].PINCFG[bus->mosi_pin].bit.PMUXEN = 1;
  if ((bus->mosi_pin & 1u) == 0)
    PORT->Group[bus->mosi_port].PMUX[bus->mosi_pin >> 1].bit.PMUXE = PORT_PMUX_PMUXE_C_Val;
  else
    PORT->Group[bus->mosi_port].PMUX[bus->mosi_pin >> 1].bit.PMUXO = PORT_PMUX_PMUXO_C_Val;

  // SCK
  PORT->Group[bus->sck_port].PINCFG[bus->sck_pin].bit.PMUXEN = 1;
  if ((bus->sck_pin & 1u) == 0)
    PORT->Group[bus->sck_port].PMUX[bus->sck_pin >> 1].bit.PMUXE = PORT_PMUX_PMUXE_C_Val;
  else
    PORT->Group[bus->sck_port].PMUX[bus->sck_pin >> 1].bit.PMUXO = PORT_PMUX_PMUXO_C_Val;

  // MISO
  PORT->Group[bus->miso_port].PINCFG[bus->miso_pin].bit.PMUXEN = 1;
  if ((bus->miso_pin & 1u) == 0)
    PORT->Group[bus->miso_port].PMUX[bus->miso_pin >> 1].bit.PMUXE = PORT_PMUX_PMUXE_C_Val;
  else
    PORT->Group[bus->miso_port].PMUX[bus->miso_pin >> 1].bit.PMUXO = PORT_PMUX_PMUXO_C_Val;
}

void spi_enable_gclk(spi_t* bus) {
  // Power & core clock for the selected SERCOM (core clock is enough for SPI)
  if (bus->sercom == SERCOM0) { PM->APBCMASK.bit.SERCOM0_ = 1;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM0_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  } else if (bus->sercom == SERCOM1) { PM->APBCMASK.bit.SERCOM1_ = 1;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM1_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  } else if (bus->sercom == SERCOM2) { PM->APBCMASK.bit.SERCOM2_ = 1;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM2_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  } else if (bus->sercom == SERCOM3) { PM->APBCMASK.bit.SERCOM3_ = 1;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM3_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  } else if (bus->sercom == SERCOM4) { PM->APBCMASK.bit.SERCOM4_ = 1;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM4_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  } else if (bus->sercom == SERCOM5) { PM->APBCMASK.bit.SERCOM5_ = 1;
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM5_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN;
  }
  while (GCLK->STATUS.bit.SYNCBUSY) {}
}

void spi_set_baud(spi_t* bus, uint32_t hz) {
  // BAUD = fref / (2*hz) - 1, with fref = 48 MHz (GCLK0)
  const uint32_t fref = 48000000u;
  uint32_t baud = (fref / (2u * hz)) - 1u;
  if (baud > 255u) baud = 255u;
  bus->sercom->SPI.BAUD.reg = (uint8_t)baud;
  spi_wait_sync(bus);
}

void spi_master_init(spi_t* bus) {
  spi_enable_gclk(bus);
  spi_pins_init(bus);
  spi_chip_select_init(bus);

  // Reset
  bus->sercom->SPI.CTRLA.bit.ENABLE = 0;
  spi_wait_sync(bus);
  bus->sercom->SPI.CTRLA.bit.SWRST = 1;
  while (bus->sercom->SPI.CTRLA.bit.SWRST || bus->sercom->SPI.SYNCBUSY.bit.SWRST) {}

  // Master, Mode0 (CPOL=0, CPHA=0), DOPO=0 (MOSI=PAD0, SCK=PAD1), DIPO=3 (MISO=PAD3), MSB first
  bus->sercom->SPI.CTRLA.reg =
      SERCOM_SPI_CTRLA_MODE(3)   // Master
    | SERCOM_SPI_CTRLA_DOPO(0)   // DO: PAD0 MOSI, PAD1 SCK, PAD2 SS (unused)
    | SERCOM_SPI_CTRLA_DIPO(3);   // DI: PAD3 (MISO)
    //| SERCOM_SPI_CTRLA_CPOL(0)
    //| SERCOM_SPI_CTRLA_CPHA(0);

  // Enable receiver
  bus->sercom->SPI.CTRLB.reg = SERCOM_SPI_CTRLB_RXEN;
  spi_wait_sync(bus);

  spi_set_baud(bus, spi_s1.init_hz);

  // Enable
  bus->sercom->SPI.CTRLA.bit.ENABLE = 1;
  spi_wait_sync(bus);

  (void)bus->sercom->SPI.DATA.reg; // dummy read
}

