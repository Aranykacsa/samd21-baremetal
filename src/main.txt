// samd21_aht20_sercom3.c
#include "samd21.h"
#include "system_samd21.h"
#include <stdint.h>
#include <stdio.h>

#include "i2c.h"

/* ==================== SysTick + delays ==================== */
static volatile uint32_t g_ms_ticks = 0;

void SysTick_Handler(void) { g_ms_ticks++; }

/* 1 kHz SysTick */
static inline void systick_init(void) {
  SysTick_Config(SystemCoreClock / 1000u);
}

/* Millisecond delay (rollover-safe) */
static inline void delay_ms(uint32_t ms) {
  uint32_t start = g_ms_ticks;
  while ((uint32_t)(g_ms_ticks - start) < ms) { /* busy wait */ }
}

/* Microsecond delay (coarse, cycles) */
static inline void delay_us(uint32_t us) {
  uint32_t cycles = (SystemCoreClock / 1000000u) * us / 5u; // rough tuning
  while (cycles--) { __asm__ volatile ("nop"); }
}


/* ==================== AHT20 (blocking, linear) ==================== */
static uint8_t aht20_init(uint8_t addr7) {
  const uint8_t seq[3] = {0xBE,0x08,0x00};
  int rc = i2c_write(addr7, seq, 3);
  delay_ms(100);                                // one-time calibration
  return (rc == 0) ? 0 : 1;
}

static uint8_t aht20_read(uint8_t addr7, float *temp_c, float *hum_pct) {
  const uint8_t trig[3] = {0xAC,0x33,0x00};
  if (i2c_write(addr7, trig, 3) != 0) {
    // recover once
    i2c_swrst_reinit();
    delay_ms(2);
    if (i2c_write(addr7, trig, 3) != 0) return 1;
  }

  delay_ms(90);                                 // conservative conversion wait

  uint8_t d[6];
  if (i2c_read(addr7, d, 6) != 0) {
    i2c_swrst_reinit();
    delay_ms(2);
    if (i2c_read(addr7, d, 6) != 0) return 2;
  }

  if (d[0] & 0x80) {                            // still busy
    delay_ms(10);
    if (i2c_read(addr7, d, 6) != 0) return 3;
    if (d[0] & 0x80) return 4;
  }

  /* Correct 20-bit extraction */
  uint32_t humRaw  = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t tempRaw = ((uint32_t)(d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];

  *hum_pct = (humRaw  * 100.0f) / 1048576.0f;
  *temp_c  = (tempRaw * 200.0f) / 1048576.0f - 50.0f;
  return 0;
}

int main(void) {
  SystemInit();                      // 48 MHz DFLL
  systick_init();                    // SysTick 1 kHz

  /* LED PA17 for heartbeat (Feather M0 D13) */
  PM->APBBMASK.reg |= PM_APBBMASK_PORT;
  PORT->Group[0].DIRSET.reg = (1u << 17);

  i2c_init(400000);

  const uint8_t AHT_ADDR = 0x38;
  if (aht20_init(AHT_ADDR) != 0) {
    printf("AHT20 init FAILED\r\n");
  } else {
    printf("AHT20 init OK\r\n");
  }

  float t, h;
  uint32_t next = g_ms_ticks;

  while (1) {
    if ((int32_t)(g_ms_ticks - next) >= 0) {
      next += 1000;                  // 1 Hz

      uint8_t rc = aht20_read(AHT_ADDR, &t, &h);
      if (rc == 0) {
        printf("T=%.1f C  RH=%.1f %%\r\n", (double)t, (double)h);
      } else {
        printf("AHT20 read err=%u\r\n", rc);
      }

      PORT->Group[0].OUTTGL.reg = (1u << 17);
    }
  }
}