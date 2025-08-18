// ========================= i2c_wire.h =========================
// Bare‑metal CMSIS I²C master for SAMD21 (SERCOM in I2C master mode)
// C API (no C++/Arduino). Default mapping: SERCOM3 on PA22(SDA)/PA23(SCL).
//
// Usage example:
//   #include "i2c_wire.h"
//   int main(void){
//     i2c_begin(100000);                 // 100 kHz
//     uint8_t who;
//     i2c_begin_tx(0x68);                // write register address (repeated START)
//     uint8_t reg = 0x75;
//     i2c_write(&reg, 1);
//     i2c_end_tx(false);                 // no STOP -> repeated START
//     i2c_request_from(0x68, 1, true);   // read 1 byte, STOP at end
//     int r = i2c_read();
//     if (r >= 0) who = (uint8_t)r;
//     while(1){}
//   }
//
// Configure pins/SERCOM by editing the macros below *before* including this header
// in one translation unit (or override via -D at compile time).

#ifndef I2C_WIRE_H
#define I2C_WIRE_H

#include <stdint.h>
#include <stddef.h>
#include "samd21.h"

// ---------- Configuration (override as needed) ----------
#ifndef I2C_SERCOM
#  define I2C_SERCOM                SERCOM3
#  define I2C_GCLK_ID_CORE          SERCOM3_GCLK_ID_CORE
#  define I2C_PM_APBCMASK_BIT       PM_APBCMASK_SERCOM3
#  define I2C_SDA_PORT              0u    // 0 = PORTA, 1 = PORTB
#  define I2C_SDA_PIN               22u   // PA22 (Arduino 20)
#  define I2C_SCL_PORT              0u
#  define I2C_SCL_PIN               23u   // PA23 (Arduino 21)
#  define I2C_PMUX_FUNC             2u    // Function C for SERCOM on D21
#endif

#ifndef I2C_FREF_HZ
#  define I2C_FREF_HZ               (48000000u) // SERCOM core clock
#endif

#ifndef I2C_DEFAULT_HZ
#  define I2C_DEFAULT_HZ            (100000u)
#endif

#ifndef I2C_TX_BUF_SZ
#  define I2C_TX_BUF_SZ             64u
#endif
#ifndef I2C_RX_BUF_SZ
#  define I2C_RX_BUF_SZ             64u
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Initialize I2C master with target frequency (e.g., 100000 or 400000)
void     i2c_begin(uint32_t hz);
// Change I2C clock on the fly
void     i2c_set_clock(uint32_t hz);

// Transaction API (Arduino-like)
void     i2c_begin_tx(uint8_t addr7);
size_t   i2c_write(const uint8_t *data, size_t len);
size_t   i2c_write_byte(uint8_t b);
// end transmission; sendStop=true sends STOP, false keeps bus for repeated START
uint8_t  i2c_end_tx(int sendStop);

// Request to read len bytes; returns number actually available to read
size_t   i2c_request_from(uint8_t addr7, size_t len, int sendStop);
// Read one byte from the internal receive buffer; returns -1 if none
int      i2c_read(void);
// Bytes remaining in the read buffer
size_t   i2c_available(void);

#ifdef __cplusplus
}
#endif

#endif // I2C_WIRE_H
