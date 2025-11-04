#ifndef I2C_SLAVE_H
#define I2C_SLAVE_H

#include "variables.h"

/**
 * @brief Initialize an I2C slave on the given bus.
 * @param bus   I2C bus handle
 * @param addr  7-bit address this device should respond to
 */
void i2c_slave_init(i2c_t* bus, uint8_t addr);

/**
 * @brief Polling handler for slave transactions.
 *        Call this from your main loop or from an interrupt wrapper.
 */
void i2c_slave_task(i2c_t* bus);


#endif /* I2C_SLAVE_H */
