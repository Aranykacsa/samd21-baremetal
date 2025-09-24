#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include "system_samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"
#include "board.h"
#include "storage.h"

int main(void) {

  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);

  printf("INIT\r\n");
  setup_board(); 
  setup_peripherals();

  printf("init started\r\n");

  init_log_sector();

  printf("init finished\r\n");


  /*TEST STORAGE*/
  uint8_t buffer[buffer_size];
  for(uint16_t i = 0; i < buffer_size; i++) {
    if(i < 256) buffer[i] = i;
    else buffer[i] = i - 256;
  }
  
  printf("buffer filled\r\n");

  uint8_t status;

  status = raid_u8bit_values(buffer, sizeof(buffer));
  printf("status buffer 1: %d\r\n", status);
  status = raid_u8bit_values(buffer, sizeof(buffer));
  printf("status buffer 2: %d\r\n", status);
  status = raid_u8bit_values(buffer, sizeof(buffer));
  printf("status buffer 3: %d\r\n", status);

  /*status = test_save_msg();
  printf("status test: %d\r\n", status);*/
}