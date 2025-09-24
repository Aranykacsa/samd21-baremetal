#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "samd21.h"
#include "variables.h"

uint8_t  i2c_s_init(i2c_t* bus, uint8_t addr, uint8_t addr_mask);
void i2c_s_enable(i2c_t* bus);
void i2c_s_disable(i2c_t* bus);
uint8_t  i2c_s_swrst_reinit(i2c_t* bus, uint8_t addr, uint8_t addr_mask);
uint8_t  i2c_s_wait_address(i2c_t* bus, uint32_t timeout_ms, uint8_t* is_read);
uint8_t  i2c_s_read(i2c_t* bus, uint8_t* buffer, size_t len, uint32_t timeout_ms);
uint8_t  i2c_s_write(i2c_t* bus, const uint8_t* buffer, size_t len, uint32_t timeout_ms);
uint8_t  i2c_s_wait_stop(i2c_t* bus, uint32_t timeout_ms);
void i2c_s_force_idle(i2c_t* bus);

#endif // I2C_SLAVE_H
