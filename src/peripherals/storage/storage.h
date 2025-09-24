#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stddef.h>

uint8_t setup_storage(void);
uint8_t init_log_sector(void);
uint8_t save_msg(uint8_t* msg);
uint8_t test_save_msg(void);

uint8_t raid_u8bit_values(uint8_t* buffer, size_t len);
uint8_t save_u8bit_values(uint8_t* buffer, size_t len);
/*uint8_t save_8bit_values(int8_t* buffer);

uint8_t save_u16bit_values(uint16_t* buffer);
uint8_t save_16bit_values(int16_t* buffer);

uint8_t save_u32bit_values(uint32_t* buffer);
uint8_t save_32bit_values(int32_t* buffer);*/
#endif /* STORAGE_H */