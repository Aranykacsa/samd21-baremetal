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

// Initialize SERCOM3 I2C master on PA22(SDA)/PA23(SCL).
// Pass 100000 for 100 kHz (default if 0) or 400000 for 400 kHz.
// Returns 0 on success.
int i2c_init(uint32_t hz);

// Software reset + reinit with previous speed. Use for recovery.
void i2c_swrst_reinit(void);

// Change I2C speed on the fly (recomputes BAUD). Returns 0 on success.
int i2c_set_speed(uint32_t hz);

// Write 'len' bytes to 7-bit address. Sends STOP. Returns 0 on success.
// Error codes: 1 addr timeout, 2 addr NACK, 3 data timeout, 4 data NACK.
int i2c_write(uint8_t addr7, const uint8_t *buf, uint32_t len);

// Read 'len' bytes from 7-bit address. Sends STOP. Returns 0 on success.
// Error codes: 1 addr timeout, 2 data timeout.
int i2c_read(uint8_t addr7, uint8_t *buf, uint32_t len);

// Force bus IDLE and clear sticky errors (BUSERR/ARBLOST).
void i2c_force_idle(void);

#endif // I2C_H
