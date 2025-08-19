// samd21_aht20_sercom3.c
#include "samd21.h"
#include "system_samd21.h"
#include <stdint.h>

#define PIN_SERIAL1_RX       (0ul)
#define PIN_SERIAL1_TX       (1ul)
#define PAD_SERIAL1_TX       (UART_TX_PAD_2)
#define PAD_SERIAL1_RX       (SERCOM_RX_PAD_3)

/* ---------- accurate 1 ms tick ---------- */
static volatile uint32_t g_ms = 0;
void SysTick_Handler(void) { g_ms++; }
static inline void delay_ms(uint32_t ms) {
  uint32_t start = g_ms;
  while ((g_ms - start) < ms) { __NOP(); }
}

/* ---------- AHT20 helpers ---------- */
static uint8_t aht20_init(uint8_t addr7) {
  // 0xBE 0x08 0x00 — calibration/initialization
  i2c_begin_tx(addr7);
  uint8_t seq[3] = {0xBE, 0x08, 0x00};
  if (i2c_write(seq, 3) != 3 || i2c_end_tx(1) != 0) return 1;
  delay_ms(100);
  return 0;
}

// Return 0 on success; nonzero on error
static uint8_t aht20_measure(float *temp_c, float *hum_pct, uint8_t addr7) {
  // trigger: 0xAC 0x33 0x00
  uint8_t trig[3] = {0xAC, 0x33, 0x00};
  i2c_begin_tx(addr7);
  if (i2c_write(trig, 3) != 3 || i2c_end_tx(1) != 0) return 1;

  delay_ms(85); // typical conversion time

  if (i2c_request_from(addr7, 6, 1) != 6) return 2;

  uint8_t d0 = (uint8_t)i2c_read();
  uint8_t d1 = (uint8_t)i2c_read();
  uint8_t d2 = (uint8_t)i2c_read();
  uint8_t d3 = (uint8_t)i2c_read();
  uint8_t d4 = (uint8_t)i2c_read();
  uint8_t d5 = (uint8_t)i2c_read(); // CRC (unused here)

  // Optional: if busy bit still set, small wait+retry
  if (d0 & 0x80) {
    delay_ms(10);
    if (i2c_request_from(addr7, 6, 1) != 6) return 3;
    d0 = (uint8_t)i2c_read();
    d1 = (uint8_t)i2c_read();
    d2 = (uint8_t)i2c_read();
    d3 = (uint8_t)i2c_read();
    d4 = (uint8_t)i2c_read();
    d5 = (uint8_t)i2c_read();
  }

  // Debug dump (like your Serial version)
  printf("AHT20 raw: %02X %02X %02X %02X %02X %02X\n", d0, d1, d2, d3, d4, d5);

  // 20-bit fields per datasheet:
  // Humidity  = [d1(3:0), d2, d3]  -> ((d1 & 0x0F)<<16) | (d2<<8) | d3
  // Temperature = [d3(7:4), d4, d5] -> ((d3 & 0xF0)<<12) | (d4<<8) | d5
  uint32_t humRaw  = ((uint32_t)(d1 & 0x0F) << 16) | ((uint32_t)d2 << 8) | d3;
  uint32_t tempRaw = ((uint32_t)(d3 & 0xF0) << 12) | ((uint32_t)d4 << 8) | d5;

  *hum_pct = (humRaw  * 100.0f) / 1048576.0f;
  *temp_c  = (tempRaw * 200.0f) / 1048576.0f - 50.0f;

  return 0;
}

int main(void) {

  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);     // 1 ms tick

  /* (Optional) UART for printf */
  uart_init(SERCOM5, 115200, 10, 1, 11, 3, PORT_PMUX_PMUXE_D_Val);
  setvbuf(stdout, NULL, _IONBF, 0);
}
