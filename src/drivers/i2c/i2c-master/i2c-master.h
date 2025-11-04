#ifndef I2C_MASTER_H
#define I2C_MASTER_H

#include <stdbool.h>
#include <stdint.h>
#include "variables.h"

/**
 * @brief Initialize an I2C master on the specified bus.
 * @param bus     Pointer to I2C bus configuration
 * @param f_gclk  SERCOM core clock frequency in Hz (usually 8 MHz or 48 MHz)
 */
void i2c_master_init(i2c_t* bus, uint32_t f_gclk);

/**
 * @brief Write data to a slave device.
 * @param bus   I2C bus handle
 * @param addr  7-bit slave address
 * @param data  Pointer to transmit buffer
 * @param len   Number of bytes to send
 * @return true on success, false if NACK or error
 */
bool i2c_master_write(i2c_t* bus, uint8_t addr, const uint8_t* data, uint16_t len);

/**
 * @brief Read data from a slave device.
 * @param bus   I2C bus handle
 * @param addr  7-bit slave address
 * @param data  Destination buffer
 * @param len   Number of bytes to read
 * @return true on success, false if NACK or error
 */
bool i2c_master_read(i2c_t* bus, uint8_t addr, uint8_t* data, uint16_t len);

#endif /* I2C_MASTER_H */
