# SAMD21G18A Bare-Metal Example

This is a **bare-metal C project** for the **Microchip/Atmel SAMD21G18A** (ARM Cortex-M0+).
No Arduino, ASF, or HAL — just **CMSIS** and the **SAMD21 device pack**.
---
(https://users.ece.cmu.edu/~eno/coding/CCodingStandard.html#descriptive)
---
## Features
- Target: **ATSAMD21G18A** @ 48 MHz
- Pure bare-metal + CMSIS
- Outputs `.elf`, `.bin`, `.hex`
- Flashable with **OpenOCD + CMSIS-DAP**
- Mainly for learning purposes

---

## Dependencies
> This document is not complet (probably).

### Install toolchain & debugger

Arch Linux:
```bash
sudo pacman -S arm-none-eabi-gcc arm-none-eabi-binutils arm-none-eabi-newlib openocd dos2unix
yay -S cmsis-toolbox

echo 'export PATH="/opt/cmsis-toolbox/bin:$PATH"' >> ~/.bashrc
echo 'export CMSIS_PACK_ROOT="$HOME/.cache/arm/packs"' >> ~/.bashrc
source ~/.bashrc

sudo ln -s /opt/cmsis-toolbox/bin/packchk /usr/local/bin/PackChk
```

### Verification:
```bash
command -v packchk
packchk --version
command -v cpackget
```

You have to download the zip from: 
```bash 
https://www.arm.com/technologies/cmsis
```
Then run `./gen_pack.sh` (no sudo required).

### Install **CMSIS** + **SAMD21 Device Pack** using `cpackget`:

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

Clean:
```bash
make clean
```

Build: 
```bash
make -j
```

Or both of them shortly:
```bash
make build
```


Artifacts are created in `.out/`:

```
.out/samd21g18-blinky.elf
.out/samd21g18-blinky.bin
.out/samd21g18-blinky.hex
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

or

```bash
make flash
```

For quick clean, build and flash run:
```bash
make quick
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
