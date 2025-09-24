#include "spi.h"
#include "variables.h"
#include "samd21.h"
#include "clock.h"
#include <stdio.h>
#include <stdint.h>

/*** ---- RETURN CODES ---- ***/
#define SPI_OK        0x00
#define SPI_ERR_SYNC  0x01
#define SPI_ERR_PARAM 0x02

/*** ---- DEBUG ---- ***/
#ifndef SPI_DEBUG
#define SPI_DEBUG 1
#endif

#if SPI_DEBUG
  #define SPI_LOG(fmt, ...)  printf("[SPI] " fmt "\r\n", ##__VA_ARGS__)
#else
  #define SPI_LOG(...)       do{}while(0)
#endif

/* --- LOW LEVEL HELPERS --- */
uint8_t spi_wait_sync(spi_t* bus) {
    uint32_t timeout = 100000;
    while (bus->sercom->SPI.SYNCBUSY.reg) {
        if (--timeout == 0) {
            SPI_LOG("SYNC timeout");
            return SPI_ERR_SYNC;
        }
    }
    return SPI_OK;
}

/* --- PUBLIC API --- */
uint8_t SD_CS_HIGH(spi_t* bus) {
    PORT->Group[bus->cs_port].OUTSET.reg = (1u << bus->cs_pin);
    return SPI_OK;
}

uint8_t SD_CS_LOW(spi_t* bus) {
    PORT->Group[bus->cs_port].OUTCLR.reg = (1u << bus->cs_pin);
    return SPI_OK;
}

uint8_t spi_txrx(spi_t* bus, uint8_t v, uint8_t *out) {
    // Wait until Data Register Empty
    uint32_t timeout = 100000;
    while (!bus->sercom->SPI.INTFLAG.bit.DRE) {
        if (--timeout == 0) {
            SPI_LOG("TX DRE timeout");
            return SPI_ERR_SYNC;
        }
    }
    bus->sercom->SPI.DATA.reg = v;

    // Wait until Receive Complete
    timeout = 100000;
    while (!bus->sercom->SPI.INTFLAG.bit.RXC) {
        if (--timeout == 0) {
            SPI_LOG("RX timeout");
            return SPI_ERR_SYNC;
        }
    }
    *out = (uint8_t)bus->sercom->SPI.DATA.reg;
    return SPI_OK;
}

uint8_t spi_chip_select_init(spi_t* bus) {
    PM->APBBMASK.bit.PORT_ = 1;
    PORT->Group[bus->cs_port].DIRSET.reg = (1u << bus->cs_pin);
    SD_CS_HIGH(bus);
    return SPI_OK;
}

uint8_t spi_pins_init(spi_t* bus) {
    PM->APBBMASK.bit.PORT_ = 1;

    // MOSI
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

    return SPI_OK;
}

uint8_t spi_enable_gclk(spi_t* bus) {
    if      (bus->sercom == SERCOM0) { PM->APBCMASK.bit.SERCOM0_ = 1;
      GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM0_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN; }
    else if (bus->sercom == SERCOM1) { PM->APBCMASK.bit.SERCOM1_ = 1;
      GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM1_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN; }
    else if (bus->sercom == SERCOM2) { PM->APBCMASK.bit.SERCOM2_ = 1;
      GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM2_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN; }
    else if (bus->sercom == SERCOM3) { PM->APBCMASK.bit.SERCOM3_ = 1;
      GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM3_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN; }
    else if (bus->sercom == SERCOM4) { PM->APBCMASK.bit.SERCOM4_ = 1;
      GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM4_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN; }
    else if (bus->sercom == SERCOM5) { PM->APBCMASK.bit.SERCOM5_ = 1;
      GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM5_GCLK_ID_CORE) | GCLK_CLKCTRL_GEN_GCLK0 | GCLK_CLKCTRL_CLKEN; }
    else {
      SPI_LOG("Invalid SERCOM ptr");
      return SPI_ERR_PARAM;
    }

    while (GCLK->STATUS.bit.SYNCBUSY) {}
    return SPI_OK;
}

uint8_t spi_set_baud(spi_t* bus, uint32_t hz) {
    const uint32_t fref = 48000000u;
    uint32_t baud = (fref / (2u * hz)) - 1u;
    if (baud > 255u) baud = 255u;
    bus->sercom->SPI.BAUD.reg = (uint8_t)baud;
    return spi_wait_sync(bus);
}

uint8_t spi_master_init(spi_t* bus) {
    if (spi_enable_gclk(bus)) return SPI_ERR_SYNC;
    if (spi_pins_init(bus))   return SPI_ERR_SYNC;
    if (spi_chip_select_init(bus)) return SPI_ERR_SYNC;

    // Reset
    bus->sercom->SPI.CTRLA.bit.ENABLE = 0;
    if (spi_wait_sync(bus)) return SPI_ERR_SYNC;
    bus->sercom->SPI.CTRLA.bit.SWRST = 1;
    while (bus->sercom->SPI.CTRLA.bit.SWRST || bus->sercom->SPI.SYNCBUSY.bit.SWRST) {}

    // Master, Mode0, DOPO=0, DIPO=3
    bus->sercom->SPI.CTRLA.reg =
        SERCOM_SPI_CTRLA_MODE(3)   // Master
      | SERCOM_SPI_CTRLA_DOPO(0)   // MOSI=PAD0, SCK=PAD1
      | SERCOM_SPI_CTRLA_DIPO(3);  // MISO=PAD3
    bus->sercom->SPI.CTRLB.reg = SERCOM_SPI_CTRLB_RXEN;
    if (spi_wait_sync(bus)) return SPI_ERR_SYNC;

    if (spi_set_baud(bus, bus->init_hz)) return SPI_ERR_SYNC;

    bus->sercom->SPI.CTRLA.bit.ENABLE = 1;
    if (spi_wait_sync(bus)) return SPI_ERR_SYNC;

    (void)bus->sercom->SPI.DATA.reg; // dummy read
    SPI_LOG("SPI init done");
    return SPI_OK;
}
