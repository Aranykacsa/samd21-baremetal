#include "samd21.h"
#include "system_samd21.h"

static volatile uint16_t g_duty = 0;
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
}
