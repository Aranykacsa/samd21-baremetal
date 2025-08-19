#include "samd21.h"
#include "system_samd21.h"
#include "i2c.h"
#include "uart.h"
#include <stdio.h>
#include <stdint.h>

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
<<<<<<< HEAD
  SystemInit();               // make sure you’re really at 48 MHz
  setvbuf(stdout, NULL, _IONBF, 0);   // unbuffered stdout
  setvbuf(stderr, NULL, _IONBF, 0);
  i2c_begin(100000);          // SERCOM3 on PA22/PA23 (default in i2c_wire.h)
  uart_init(SERCOM5, 115200, 10, 1, 11, 3, PORT_PMUX_PMUXE_D_Val);
=======
  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);     // 1 ms tick

  uart_init_115200_sercom5_pa10_pa11();  // first

  // printf unbuffered
  setvbuf(stdout, NULL, _IONBF, 0);

  i2c_begin(100000);                          // your SERCOM/pins for I2C
>>>>>>> b1a797da161d67d361500eb9fa39ba28b01a54bb

  printf("Hello from Feather M0 @115200 8N1\n");

  // one-time init sequence (matches your working Wire code)
  if (aht20_init(0x38) != 0) {
    printf("AHT20 not responding during init.\n");
  } else {
    printf("AHT20 init OK\n");
  }

  float t, h;
  while (1) {
    uint8_t rc = aht20_measure(&t, &h, 0x38);
    if (rc == 0) {
      printf("A\nB\nC\n");  // should show three lines
      printf("Temperature: %.1f C | Humidity: %.1f %%\n", t, h);
    } else {
      printf("AHT20 read error: %u\n", rc);
    }
    delay_ms(1000); // 1 s
  }
}


/*static volatile uint16_t g_duty = 0;
static volatile int g_dir = 1;          // 1 = up, -1 = down
static const uint16_t TOP = 2399;       // 48MHz / 1 / (TOP+1) = 20 kHz
static const uint16_t STEP = 8;         // fade step per period (tweak speed)

static void pwm_pa17_init(void)
{
  // Bus clocks
  PM->APBCMASK.reg |= PM_APBCMASK_TCC2;

  // GCLK0 -> TCC2/TC3
  while (GCLK->STATUS.bit.SYNCBUSY) {}
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_TCC2_TC3 |
                      GCLK_CLKCTRL_GEN_GCLK0   |
                      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY) {}

  // PA17 => peripheral E (TCC2/WO[1])
  PORT->Group[0].PINCFG[17].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[17 >> 1].bit.PMUXO = MUX_PA17E_TCC2_WO1;

  // Reset + configure TCC2
  TCC2->CTRLA.bit.SWRST = 1;
  while (TCC2->SYNCBUSY.bit.SWRST) {}

  TCC2->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV1;      // full 48 MHz
  while (TCC2->SYNCBUSY.reg) {}

  TCC2->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;
  while (TCC2->SYNCBUSY.bit.WAVE) {}

  // Period + initial duty
  TCC2->PER.reg = TOP;
  while (TCC2->SYNCBUSY.bit.PER) {}

  TCC2->CC[1].reg = g_duty;
  while (TCC2->SYNCBUSY.bit.CC1) {}

  // Enable OVF interrupt (fires each PWM period)
  TCC2->INTENSET.bit.OVF = 1;
  NVIC_SetPriority(TCC2_IRQn, 3);
  NVIC_EnableIRQ(TCC2_IRQn);

  // Go
  TCC2->CTRLA.bit.ENABLE = 1;
  while (TCC2->SYNCBUSY.bit.ENABLE) {}
}

void TCC2_Handler(void)
{
  // Clear overflow flag
  TCC2->INTFLAG.reg = TCC_INTFLAG_OVF;

  // Ramp duty up/down
  uint16_t d = g_duty;
  if (g_dir > 0) {
    d = (d + STEP > TOP) ? TOP : d + STEP;
    if (d == TOP) g_dir = -1;
  } else {
    d = (d > STEP) ? (d - STEP) : 0;
    if (d == 0) g_dir = 1;
  }
  g_duty = d;

  // Queue duty for next period boundary
  TCC2->CCB[1].reg = d;
}

int main(void)
{
  SystemInit();
  pwm_pa17_init();
  for (;;) __WFI();   // sleep between interrupts
}*/
