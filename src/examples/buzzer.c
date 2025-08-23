#include "samd21.h"
#include "system_samd21.h"
#include <stdint.h>

/* ====== Your existing PA07 TCC1 init (2 kHz baseline) ====== */
static void tcc1_pa07_init(void){
  PM->APBBMASK.reg |= PM_APBBMASK_PORT;
  PORT->Group[0].PINCFG[7].reg = 0;
  PORT->Group[0].PINCFG[7].bit.PMUXEN = 1;                 // peripheral mux
  PORT->Group[0].PMUX[7>>1].bit.PMUXO = PORT_PMUX_PMUXO_E_Val; // PA07 odd → E (TCC1/WO[1])

  PM->APBCMASK.reg |= PM_APBCMASK_TCC1;
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_TCC0_TCC1
                    | GCLK_CLKCTRL_GEN_GCLK0
                    | GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY);

  TCC1->CTRLA.bit.ENABLE = 0; while (TCC1->SYNCBUSY.bit.ENABLE);
  TCC1->CTRLA.bit.SWRST  = 1; while (TCC1->SYNCBUSY.bit.SWRST);

  TCC1->WAVE.reg  = TCC_WAVE_WAVEGEN_NPWM;  while (TCC1->SYNCBUSY.bit.WAVE);
  TCC1->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV64;
  while (TCC1->SYNCBUSY.bit.CTRLB);

  TCC1->PER.reg = 374; while (TCC1->SYNCBUSY.bit.PER);      // ~2 kHz default
  TCC1->CC[1].reg = 187; while (TCC1->SYNCBUSY.bit.CC1);

  TCC1->CTRLA.bit.ENABLE = 1; while (TCC1->SYNCBUSY.bit.ENABLE);
}

/* ====== Low-jitter updates via buffered regs ====== */
static inline void tcc1_push_update(void){
  TCC1->CTRLBSET.reg = TCC_CTRLBSET_CMD_UPDATE;
  while (TCC1->SYNCBUSY.bit.CTRLB);
}

/* Set frequency (Hz) & duty% on TCC1/WO[1] */
static void buzzer_set(uint32_t hz, uint8_t duty_pct){
  if (hz == 0 || duty_pct == 0){
    while (TCC1->SYNCBUSY.bit.CC1);
    TCC1->CCB[1].reg = 0;
    tcc1_push_update();
    return;
  }
  const uint32_t f = 48000000u, presc = 64u;
  uint32_t per = f/(presc*hz);
  if (per) per -= 1;
  if (per > 0xFFFFFFu) per = 0xFFFFFFu;

  while (TCC1->SYNCBUSY.bit.PER);
  TCC1->PERB.reg = per;

  uint32_t cc = ((per+1) * duty_pct) / 100u;
  if (cc > per) cc = per;
  while (TCC1->SYNCBUSY.bit.CC1);
  TCC1->CCB[1].reg = cc;

  tcc1_push_update();
}

/* Crude delays (48 MHz core) */
static inline void delay_cycles(volatile uint32_t n){ while(n--) __NOP(); }
static inline void delay_ms(uint32_t ms){ delay_cycles(48000u * ms); }

typedef struct { uint16_t freq_hz; uint16_t dur_ms; } note_t;

#define NOTE_C4  262
#define NOTE_CS4 277
#define NOTE_D4  294
#define NOTE_DS4 311
#define NOTE_E4  330
#define NOTE_F4  349
#define NOTE_FS4 370
#define NOTE_G4  392
#define NOTE_GS4 415
#define NOTE_A4  440
#define NOTE_AS4 466
#define NOTE_B4  494

#define NOTE_C5  523
#define NOTE_CS5 554
#define NOTE_D5  587
#define NOTE_DS5 622
#define NOTE_E5  659
#define NOTE_F5  698
#define NOTE_FS5 740
#define NOTE_G5  784
#define NOTE_GS5 831
#define NOTE_A5  880
#define NOTE_AS5 932
#define NOTE_B5  988

#define NOTE_C6  1047
#define NOTE_CS6 1109
#define NOTE_D6  1175
#define NOTE_DS6 1245
#define NOTE_E6  1319
#define NOTE_F6  1397
#define NOTE_FS6 1480
#define NOTE_G6  1568
#define NOTE_GS6 1661
#define NOTE_A6  1760
#define NOTE_AS6 1865
#define NOTE_B6  1976

static const note_t fur_elise[] = {
  {NOTE_E5,200}, {NOTE_DS5,200}, {NOTE_E5,200}, {NOTE_DS5,200}, {NOTE_E5,200},
  {NOTE_B4,200}, {NOTE_D5,200}, {NOTE_C5,200}, {NOTE_A4,400},

  {NOTE_A4,200}, {NOTE_C4,200}, {NOTE_E4,200}, {NOTE_A4,200}, {NOTE_B4,400},

  {NOTE_E4,200}, {NOTE_GS4,200}, {NOTE_B4,200}, {NOTE_C5,200}, {NOTE_E5,400},

  {NOTE_E4,200}, {NOTE_E5,200}, {NOTE_DS5,200}, {NOTE_E5,200}, {NOTE_DS5,200},
  {NOTE_E5,200}, {NOTE_B4,200}, {NOTE_D5,200}, {NOTE_C5,200}, {NOTE_A4,400}
};



static void play_notes(const note_t* s, uint32_t n, uint8_t duty, uint8_t gap_ms) {
  for (uint32_t i=0; i<n; ++i){
    buzzer_set(s[i].freq_hz, duty);
    delay_ms(s[i].dur_ms);
    buzzer_set(0, 0);                 // brief gap for articulation
    delay_ms(gap_ms);
  }
}

int main(void){
  SystemInit();
    SysTick_Config(SystemCoreClock / 1000);     // 1 ms tick
  tcc1_pa07_init();

  for(;;){
    play_notes(fur_elise, sizeof(fur_elise)/sizeof(fur_elise[0]), 60, 10);

    delay_ms(2000);
  }
}

