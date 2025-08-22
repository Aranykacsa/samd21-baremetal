#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "samd21.h"

typedef struct {
  Sercom*  sercom;
  uint8_t  sda_port, sda_pin;
  uint8_t  scl_port, scl_pin;
  uint8_t  pmux_func;       // A=0..H=7 (I2C is typically C=2)
  uint32_t src_clk_hz;      // e.g. 48'000'000
  uint32_t hz;              // bus speed (e.g. 100k / 400k)
  uint32_t trise_ns;        // rise-time estimate (e.g. 120 ns), 0 to skip comp
} i2c_t;

/* Initialize one I2C bus on an arbitrary SERCOM+pins. */
int  i2c_init(i2c_t* bus,
              Sercom* sercom, uint32_t hz,
              uint8_t sda_port, uint8_t sda_pin,
              uint8_t scl_port, uint8_t scl_pin,
              uint8_t pmux_func, uint32_t src_clk_hz);

/* Runtime controls & transfers */
int  i2c_set_speed(i2c_t* bus, uint32_t hz);
void i2c_force_idle(i2c_t* bus);
void i2c_swrst_reinit(i2c_t* bus);
int  i2c_write(i2c_t* bus, uint8_t addr7, const uint8_t* buf, uint32_t len);
int  i2c_read (i2c_t* bus, uint8_t addr7,       uint8_t* buf);

#endif // I2C_H
