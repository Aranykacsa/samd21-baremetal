#include "samd21.h"
#include "system_samd21.h"
#include "i2c.h"
#include "uart.h"
#include <stdint.h>
#include <stddef.h>
#include <sys/unistd.h>
#include <stdio.h>

static void delay_ms(uint32_t ms) {
  // crude busy-wait @48 MHz (≈10 cycles per loop): ~0.21 ms per 1000 iters
  // tune or replace with SysTick/TC as you like
  volatile uint32_t n = ms * 230000;
  while (n--) __asm__ __volatile__("nop");
}

// Returns 0 on success; nonzero = error
uint8_t aht20_measure(float *temp, float *hum, uint8_t addr7) {
  uint8_t cmd[3] = {0xAC, 0x33, 0x00};

  i2c_begin_tx(addr7);
  if (i2c_write(cmd, 3) != 3 || i2c_end_tx(1) != 0) return 1;

  delay_ms(85); // AHT20 typical conversion time (~80 ms)

  if (i2c_request_from(addr7, 6, 1) != 6) return 2;

  uint8_t d0 = (uint8_t)i2c_read();
  uint8_t d1 = (uint8_t)i2c_read();
  uint8_t d2 = (uint8_t)i2c_read();
  uint8_t d3 = (uint8_t)i2c_read();
  uint8_t d4 = (uint8_t)i2c_read();
  uint8_t d5 = (uint8_t)i2c_read(); // CRC (ignored here)

  // Optional: if (d0 & 0x80) return 3; // sensor still busy

  uint32_t humRaw  = ((uint32_t)(d1 & 0x0F) << 16) | ((uint32_t)d2 << 8) | d3;
  uint32_t tempRaw = ((uint32_t)(d3 & 0xF0) << 12) | ((uint32_t)d4 << 8) | d5;

  *hum  = (humRaw  * 100.0f) / 1048576.0f;        // 20-bit
  *temp = (tempRaw * 200.0f) / 1048576.0f - 50.0f;

  return 0;
}

int main(void) {
  SystemInit();               // make sure you’re really at 48 MHz
  setvbuf(stdout, NULL, _IONBF, 0);   // unbuffered stdout
  setvbuf(stderr, NULL, _IONBF, 0);
  i2c_begin(100000);          // SERCOM3 on PA22/PA23 (default in i2c_wire.h)
  uart_init(SERCOM5, 115200, 10, 1, 11, 3, PORT_PMUX_PMUXE_D_Val);

  printf("Hello from Feather M0 @115200 8N1\n");

  float t, h;
  uint8_t rc = aht20_measure(&t, &h, 0x38);  // AHT20 default addr = 0x38
  (void)rc;                                  // use rc/t/h as needed

  while (1) { __NOP(); }
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
