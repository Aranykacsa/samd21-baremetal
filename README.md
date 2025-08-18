# SAMD21G18A Bare-Metal Example (PWM Fade on PA17)

This is a **bare-metal C project** for the **Microchip/Atmel SAMD21G18A** (ARM Cortex-M0+).
It fades the on-board LED on **PA17** using **TCC2 WO[1] PWM**.
No Arduino, ASF, or HAL — just **CMSIS** and the **SAMD21 device pack**.

---

## Features
- Target: **ATSAMD21G18A** @ 48 MHz
- PWM output on **PA17** (TCC2/WO[1])
- Smooth fading via double-buffered duty (`CCBUF`)
- Pure bare-metal + CMSIS
- Outputs `.elf`, `.bin`, `.hex`
- Flashable with **OpenOCD + CMSIS-DAP**

---

## Dependencies

Original source of information: `https://www.arm.com/technologies/cmsis`

Install toolchain & debugger (Fedora example):

```bash
sudo dnf install -y arm-none-eabi-gcc-cs arm-none-eabi-newlib openocd dos2unix
```

Install **CMSIS** + **SAMD21 Device Pack** using `cpackget`:

```bash
cpackget init https://www.keil.com/pack/index.pidx
cpackget add ARM::CMSIS
cpackget add Keil::SAMD21_DFP@1.3.1
```

Packs will be installed under:

```
~/.cache/arm/packs/ARM/CMSIS/<ver>/CMSIS/Core/Include
~/.cache/arm/packs/Keil/SAMD21_DFP/1.3.1/Device/SAMD21A/Include
```

---

## Project Structure

```
samd21g18-baremetal/
├─ build/
├─ out/
├─ env/
  ├─ syscalls_min.c
  ├─ startup_samd21_gcc.c
  ├─ samd21g18a.ld
  ├─ system_samd21.c
├─ src/
  ├─ main.c
├─ Makefile
```

---

## Building

```bash
make clean && make -j
```

Artifacts are created in `out/`:

```
out/samd21g18-blinky.elf
out/samd21g18-blinky.bin
out/samd21g18-blinky.hex
```

If you hit:

```
samd21g18a.ld: syntax error
```

fix CRLF line endings:

```bash
dos2unix samd21g18a.ld
```

---

## Flashing with OpenOCD

Example (using CMSIS-DAP, SAMD21G18A, offset 0x2000):

```bash
openocd -f interface/cmsis-dap.cfg -f target/at91samdXX.cfg \
  -c "program out/samd21g18-blinky.bin 0x00002000 verify reset exit"
```

If you prefer ELF directly:

```bash
openocd -f interface/cmsis-dap.cfg -f target/at91samdXX.cfg \
  -c "program out/samd21g18-blinky.elf verify reset exit"
```

Easy to use command
```bash
make clean && make -j && make flash
```

---

## main.c (PWM Fade Example)

```c
#include "samd21.h"
#include "system_samd21.h"

static void pwm_pa17_init(uint16_t top, uint16_t duty) {
  PM->APBCMASK.reg |= PM_APBCMASK_TCC2;

  while (GCLK->STATUS.bit.SYNCBUSY) { }
  GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID_TCC2_TC3 |
                      GCLK_CLKCTRL_GEN_GCLK0   |
                      GCLK_CLKCTRL_CLKEN;
  while (GCLK->STATUS.bit.SYNCBUSY) { }

  PORT->Group[0].PINCFG[17].bit.PMUXEN = 1;
  PORT->Group[0].PMUX[17 >> 1].bit.PMUXO = MUX_PA17E_TCC2_WO1;

  TCC2->CTRLA.bit.SWRST = 1;
  while (TCC2->SYNCBUSY.bit.SWRST) { }

  TCC2->CTRLA.reg = TCC_CTRLA_PRESCALER_DIV64;
  while (TCC2->SYNCBUSY.reg) { }

  TCC2->WAVE.reg = TCC_WAVE_WAVEGEN_NPWM;
  while (TCC2->SYNCBUSY.bit.WAVE) { }

  TCC2->PER.reg = top;
  while (TCC2->SYNCBUSY.bit.PER) { }

  TCC2->CC[1].reg = duty;
  while (TCC2->SYNCBUSY.bit.CC1) { }

  TCC2->CTRLA.bit.ENABLE = 1;
  while (TCC2->SYNCBUSY.bit.ENABLE) { }
}

int main(void) {
  SystemInit();

  const uint16_t top = 749;        // ≈1 kHz
  pwm_pa17_init(top, 0);

  uint16_t d = 0;
  int dir = 1;

  for (;;) {
    TCC2->CCBUF[1].reg = d;
    while (TCC2->SYNCBUSY.bit.CCBUF1) { }

    d += dir ? 5 : -5;
    if (d >= top) { d = top; dir = 0; }
    if (d == 0)   { dir = 1; }

    for (volatile uint32_t i = 0; i < 5000; ++i) __NOP();
  }
}
```
