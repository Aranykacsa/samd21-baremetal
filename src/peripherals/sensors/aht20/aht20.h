#ifndef AHT_20_H
#define AHT_20_H

#include "variables.h"

uint8_t aht20_init(i2c_t* bus, uint8_t addr7);
uint8_t aht20_read(i2c_t* bus, uint8_t addr7, float *temp_c, float *hum_pct);

#endif /* AHT_20_H */ 