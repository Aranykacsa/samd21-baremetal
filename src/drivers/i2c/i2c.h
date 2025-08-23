#ifndef I2C_H
#define I2C_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "samd21.h"
#include "variables.h"


int  i2c_init(i2c_t* bus);
int  i2c_set_speed(i2c_t* bus, uint32_t hz);
void i2c_force_idle(i2c_t* bus);
void i2c_swrst_reinit(i2c_t* bus);
int  i2c_write(i2c_t* bus, uint8_t addr, const uint8_t* buf);
int  i2c_read (i2c_t* bus, uint8_t addr,       uint8_t* buf);

#endif // I2C_H
