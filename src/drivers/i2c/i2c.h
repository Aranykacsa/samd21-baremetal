#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "samd21.h"

// ===== Immutable per-instance config =====
typedef struct {
    Sercom*  sercom;             // SERCOMx
    uint8_t  gclk_id_core;       // SERCOMx_GCLK_ID_CORE
    uint32_t pm_apbcmask_bit;    // PM_APBCMASK_SERCOMx

    // Pinmux: SDA must be on PAD[0], SCL on PAD[1]
    uint8_t  sda_port;           // 0=PORTA, 1=PORTB
    uint8_t  sda_pin;            // e.g., PA22 -> port=0, pin=22
    uint8_t  scl_port;           // 0=PORTA, 1=PORTB
    uint8_t  scl_pin;
    uint8_t  pmux_func;          // Usually 2 (Function C) on D21
} i2c_cfg_t;

// ===== Mutable per-instance state =====
#ifndef I2C_TX_BUF_SZ
#  define I2C_TX_BUF_SZ 64u
#endif
#ifndef I2C_RX_BUF_SZ
#  define I2C_RX_BUF_SZ 64u
#endif
#ifndef I2C_FREF_HZ
#  define I2C_FREF_HZ   48000000u   // SERCOM core clock (after GCLK)
#endif

typedef struct {
    i2c_cfg_t cfg;               // copy of config (or store pointer if you prefer)
    uint32_t  hz;                // current bus speed (100k/400k)
    bool      initialized;
    bool      busy;
    uint8_t   last_error;

    // simple internal FIFOs
    uint8_t   tx[I2C_TX_BUF_SZ];
    size_t    tx_len;
    uint8_t   rx[I2C_RX_BUF_SZ];
    size_t    rx_len;
    size_t    rx_pos;
} i2c_t;

// ====== API (all instance-based) ======
#ifdef __cplusplus
extern "C" {
#endif

void   i2c_init(i2c_t* b, const i2c_cfg_t* cfg, uint32_t hz); // enable clocks, pinmux, master mode
void   i2c_set_clock(i2c_t* b, uint32_t hz);

// Transactions
void   i2c_begin_tx(i2c_t* b, uint8_t addr7);
size_t i2c_write(i2c_t* b, const uint8_t* data, size_t len);
size_t i2c_write_byte(i2c_t* b, uint8_t byte);
uint8_t i2c_end_tx(i2c_t* b, int sendStop); // 0=OK, nonzero=error

size_t i2c_request_from(i2c_t* b, uint8_t addr7, size_t len, int sendStop);
int    i2c_read(i2c_t* b);                  // -1 if none
size_t i2c_available(const i2c_t* b);

#ifdef __cplusplus
}
#endif
#endif // I2C_H
