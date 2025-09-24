#ifndef I2C_HELPER_H
#define I2C_HELPER_H

#include <stdint.h>
#include "samd21.h"
#include "system_samd21.h"
#include "variables.h"
#include "clock.h"     // must provide get_uptime() in ms

#ifndef I2C_DEFAULT_HZ
#define I2C_DEFAULT_HZ 100000u
#endif

void enable_sercom_clock(Sercom* sercom, 
						uint8_t* core, 
						uint8_t* slow);

void i2c_apply_baud(Sercom* sercom, 
					uint32_t fscl_hz,
                  	uint32_t src_hz, 
                  	uint32_t trise_ns);

void i2c_issue_stop(i2c_t* bus);

uint8_t i2c_wait_mb(i2c_t* bus, 
				uint32_t to_ms);

uint8_t i2c_wait_sb(i2c_t* bus, 
				uint32_t to_ms);

void pmux_set(uint8_t port, uint8_t pin, uint8_t func);


#endif /* I2C_HELPER_H */