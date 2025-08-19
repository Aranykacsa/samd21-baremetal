#ifndef SPI_H
#define SPI_H

#include "samd21.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// ===== Helper: simple GPIO CS control =====
typedef struct {
  uint8_t port;   // 0 -> PORTA, 1 -> PORTB
  uint8_t pin;    // 0..31
  bool    active_low;
} spi_cs_t;

static inline void spi_cs_init(spi_cs_t cs) {
  PortGroup *g = &PORT->Group[cs.port];
  g->DIRSET.reg = (1u << cs.pin);
  if (cs.active_low) g->OUTSET.reg = (1u << cs.pin); // idle high
  else               g->OUTCLR.reg = (1u << cs.pin); // idle low
}
static inline void spi_cs_assert(spi_cs_t cs) {
  PortGroup *g = &PORT->Group[cs.port];
  if (cs.active_low) g->OUTCLR.reg = (1u << cs.pin);
  else               g->OUTSET.reg = (1u << cs.pin);
}
static inline void spi_cs_deassert(spi_cs_t cs) {
  PortGroup *g = &PORT->Group[cs.port];
  if (cs.active_low) g->OUTSET.reg = (1u << cs.pin);
  else               g->OUTCLR.reg = (1u << cs.pin);
}

// ===== Core driver =====
typedef struct {
  SercomSpi *spi;           // e.g. &SERCOM0->SPI
  uint32_t   gclk_freq_hz;  // frequency of the GCLK you feed to SERCOMx core
} spi_t;

/**
 * Minimal pinmux helper.
 * Pass the PORT group (0->A, 1->B), pin number, and PMUX function value:
 *   MUX C = 2 (SERCOM), MUX D = 3 (SERCOM ALT)
 */
static inline void spi_pmux(uint8_t port, uint8_t pin, uint8_t mux)
{
  PortGroup *g = &PORT->Group[port];
  g->PINCFG[pin].bit.PMUXEN = 1;
  if ((pin & 1u) == 0) { // even
    g->PMUX[pin >> 1].bit.PMUXE = mux;
  } else {
    g->PMUX[pin >> 1].bit.PMUXO = mux;
  }
}

/**
 * Connects a GCLK to SERCOMx core and enables APBC mask.
 * gclk_id_core is SERCOMx_GCLK_ID_CORE.
 * apbc_mask_bit is PM_APBCMASK_SERCOMx.
 * gclk_src_id is the GCLK generator index you want to use (e.g. 0 for GCLK0).
 */
void spi_enable_clock(uint8_t gclk_id_core, uint32_t apbc_mask_bit, uint8_t gclk_src_id);

/**
 * Initialize SPI master:
 *  - spi          : &SERCOMx->SPI
 *  - gclk_freq_hz : frequency of the GCLK you routed (e.g. 48'000'000)
 *  - sck_hz       : desired SCK (<= gclk/2, 8-bit BAUD)
 *  - cpol/cpha    : SPI mode (0..3 derived)
 *  - dipo         : 0..3 -> which PAD is MISO
 *  - dopo         : 0..3 -> which PAD carries SCK/MOSI/SS (see datasheet)
 *  - msb_first    : true for MSB-first, false for LSB-first
 */
void spi_init(spi_t *bus, SercomSpi *spi, uint32_t gclk_freq_hz,
              uint32_t sck_hz, bool cpol, bool cpha,
              uint8_t dipo, uint8_t dopo, bool msb_first);

/** Change baud at runtime (same formula as init). */
void spi_set_baud(spi_t *bus, uint32_t sck_hz);

/** Full-duplex transfer of one byte. */
uint8_t spi_txrx(spi_t *bus, uint8_t byte);

/** Full-duplex transfer of a buffer. If rx==NULL, received bytes are discarded. */
void spi_transfer(spi_t *bus, const uint8_t *tx, uint8_t *rx, size_t len);

/** Wait until bus is idle (TX complete and not busy). */
void spi_wait_idle(spi_t *bus);

#endif // SPI_H
