#include <stdint.h>
#include "samd21.h"

/* Linker symbols */
extern unsigned long _estack;

/* Forward decls */
void Reset_Handler(void);
__attribute__((noreturn)) void Default_Handler(void);

/* Mark aliases as noreturn too to match Default_Handler and silence warnings */
#define WEAK_ALIAS_NORETURN(x) __attribute__((weak, alias(#x), noreturn))

/* Core exception handlers */
void NMI_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void HardFault_Handler(void)          WEAK_ALIAS_NORETURN(Default_Handler);
void SVC_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void PendSV_Handler(void)             WEAK_ALIAS_NORETURN(Default_Handler);
void SysTick_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);

/* Peripheral IRQ handlers (SAMD21 family superset) */
void PM_Handler(void)                 WEAK_ALIAS_NORETURN(Default_Handler);
void SYSCTRL_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void WDT_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void RTC_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void EIC_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void NVMCTRL_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void DMAC_Handler(void)               WEAK_ALIAS_NORETURN(Default_Handler);
void USB_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void EVSYS_Handler(void)              WEAK_ALIAS_NORETURN(Default_Handler);
void SERCOM0_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void SERCOM1_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void SERCOM2_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void SERCOM3_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void SERCOM4_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void SERCOM5_Handler(void)            WEAK_ALIAS_NORETURN(Default_Handler);
void TCC0_Handler(void)               WEAK_ALIAS_NORETURN(Default_Handler);
void TCC1_Handler(void)               WEAK_ALIAS_NORETURN(Default_Handler);
void TCC2_Handler(void)               WEAK_ALIAS_NORETURN(Default_Handler);
void TC3_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void TC4_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void TC5_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void TC6_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void TC7_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void ADC_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void AC_Handler(void)                 WEAK_ALIAS_NORETURN(Default_Handler);
void DAC_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void PTC_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
void I2S_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);
/* some board examples have AC1; keep alias for vector count compatibility */
void AC1_Handler(void)                WEAK_ALIAS_NORETURN(Default_Handler);

__attribute__((section(".isr_vector")))
void (* const g_pfnVectors[])(void) = {
  (void (*)(void))(&_estack), /* Initial stack pointer */
  Reset_Handler,
  NMI_Handler,
  HardFault_Handler,
  0, 0, 0, 0, 0, 0, 0,        /* Reserved */
  SVC_Handler,
  0, 0,                        /* Reserved */
  PendSV_Handler,
  SysTick_Handler,

  /* IRQs */
  PM_Handler,
  SYSCTRL_Handler,
  WDT_Handler,
  RTC_Handler,
  EIC_Handler,
  NVMCTRL_Handler,
  DMAC_Handler,
  USB_Handler,
  EVSYS_Handler,
  SERCOM0_Handler,
  SERCOM1_Handler,
  SERCOM2_Handler,
  SERCOM3_Handler,
  SERCOM4_Handler,
  SERCOM5_Handler,
  TCC0_Handler,
  TCC1_Handler,
  TCC2_Handler,
  TC3_Handler,
  TC4_Handler,
  TC5_Handler,
  TC6_Handler,
  TC7_Handler,
  ADC_Handler,
  AC_Handler,
  DAC_Handler,
  PTC_Handler,
  I2S_Handler,
  AC1_Handler
};

/* Minimal Reset: set VTOR (optional), call SystemInit(), then main() */
extern void SystemInit(void);
extern int main(void);

void Reset_Handler(void) {
  SystemInit();
  extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;
  /* copy data */
  for (uint32_t *src = &_sidata, *dst = &_sdata; dst < &_edata; )
    *dst++ = *src++;
  /* zero bss */
  for (uint32_t *dst = &_sbss; dst < &_ebss; )
    *dst++ = 0;

  /* set vector table (SAMD21 has VTOR in SCB) */
  SCB->VTOR = (uint32_t)g_pfnVectors;

  (void)main();
  /* if main returns, trap */
  Default_Handler();
}

__attribute__((noreturn)) void Default_Handler(void) {
  while (1) { __asm volatile ("wfi"); }
}
