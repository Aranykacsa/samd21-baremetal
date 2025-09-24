#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "samd21.h"
#include "variables.h"

/* ------ MASTER ------*/
uint8_t  i2c_m_init(i2c_t* bus);
uint8_t  i2c_m_set_speed(i2c_t* bus, uint32_t hz);
void i2c_m_force_idle(i2c_t* bus);
void i2c_m_swrst_reinit(i2c_t* bus);
uint8_t  i2c_m_write(i2c_t* bus, uint8_t addr, const uint8_t* buf, size_t len);
uint8_t  i2c_m_read (i2c_t* bus, uint8_t addr,       uint8_t* buf, size_t len);

#endif /* I2C_MASTER_H */