// samd21_aht20_sercom3.c
#include "samd21.h"
#include "system_samd21.h"
#include <stdint.h>
#include <stdio.h>

#include "sd-card.h"
#include "i2c.h"
#include "clock.h"
#include "uart.h"

static uint8_t buf[512];

int main(void) {
  SystemInit();
  SysTick_Config(SystemCoreClock / 1000);

  printf("\r\n[BOOT] SAMD21 SD-card SPI test\r\n");

  int rc = sd_init();
  printf("[SD] init rc=%d\r\n", rc);

  if (rc != 0) {
    printf("[SD] init failed (rc=%d)\r\n", rc);
  }
  printf("[SD] init OK, card type: %s\r\n", sd_is_sdhc() ? "SDHC/SDXC" : "SDSC");

  /* Read LBA 0 */
  rc = sd_read_block(0, buf);
  if (rc != 0) {
    printf("[SD] read block 0 failed (rc=%d)\r\n", rc);
  }
  printf("[SD] block 0 read OK\r\nFirst 16 bytes:\r\n");
  for (int i=0;i<16;i++) {
    printf("%02X ", buf[i]);
  }
  printf("\r\n");

  /* Echo write test to LBA 1 */
  for (int i=0;i<512;i++) buf[i] = (uint8_t)i;
  rc = sd_write_block(1, buf);
  if (rc != 0) {
    printf("[SD] write block 1 failed (rc=%d)\r\n", rc);
  }
  printf("[SD] block 1 write OK\r\n");
}
