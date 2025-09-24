/**
 * \file
 *
 * \brief Low-level initialization functions called upon chip startup.
 *
 * Copyright (c) 2015 Atmel Corporation. All rights reserved.
 *
 * \asf_license_start
 *
 * \page License
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. The name of Atmel may not be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * 4. This software may only be redistributed and used in connection with an
 *    Atmel microcontroller product.
 *
 * THIS SOFTWARE IS PROVIDED BY ATMEL "AS IS" AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT ARE
 * EXPRESSLY AND SPECIFICALLY DISCLAIMED. IN NO EVENT SHALL ATMEL BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \asf_license_stop
 *
 */

#include "samd21.h"

/**
 * Initial system clock frequency. The System RC Oscillator (RCSYS) provides
 *  the source for the main clock at chip startup.
 */
#define __SYSTEM_CLOCK    (48000000)

uint32_t SystemCoreClock = __SYSTEM_CLOCK;/*!< System Clock Frequency (Core Clock)*/

/**
 * Initialize the system
 *
 * @brief  Setup the microcontroller system.
 *         Initialize the System and update the SystemCoreClock variable.
 */
void SystemInit(void) {
	// enable 1 wait state required at 3.3V 48 MHz
	REG_NVMCTRL_CTRLB = NVMCTRL_CTRLB_MANW | NVMCTRL_CTRLB_RWS_HALF;

	/*****************************************************
	start external crystal XOSC32K, must disable first
	*****************************************************/
	REG_SYSCTRL_XOSC32K  = SYSCTRL_XOSC32K_STARTUP(0x7) | SYSCTRL_XOSC32K_RUNSTDBY | SYSCTRL_XOSC32K_AAMPEN | SYSCTRL_XOSC32K_EN32K | SYSCTRL_XOSC32K_XTALEN;
	REG_SYSCTRL_XOSC32K |= SYSCTRL_XOSC32K_ENABLE;
	while((REG_SYSCTRL_PCLKSR & (SYSCTRL_PCLKSR_XOSC32KRDY)) == 0);

	// CLOCK GENERATOR 3, use external XOSC32K (DFLL48M reference)
	REG_GCLK_GENCTRL = GCLK_GENCTRL_ID(0x3);
	REG_GCLK_GENDIV  = GCLK_GENDIV_DIV(1) | GCLK_GENDIV_ID(0x3);
	REG_GCLK_GENCTRL = GCLK_GENCTRL_IDC | GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_XOSC32K | GCLK_GENCTRL_ID(0x3);
	while(REG_GCLK_STATUS & GCLK_STATUS_SYNCBUSY);

	/*******************************************************************
	Initialize CPU to run at 48MHz using the DFLL48M
	Reference the XOSC32K on ClockGen3
	*******************************************************************/

	// change reference of DFLL48M to generator 3
	REG_GCLK_CLKCTRL = GCLK_CLKCTRL_CLKEN | GCLK_CLKCTRL_GEN_GCLK3 | GCLK_CLKCTRL_ID_DFLL48;
	while((REG_SYSCTRL_PCLKSR & (SYSCTRL_PCLKSR_DFLLRDY)) == 0);

	// enable open loop
	REG_SYSCTRL_DFLLCTRL = SYSCTRL_DFLLCTRL_ENABLE;
	while((REG_SYSCTRL_PCLKSR & (SYSCTRL_PCLKSR_DFLLRDY)) == 0);

	// set DFLL multipliers
	REG_SYSCTRL_DFLLMUL = SYSCTRL_DFLLMUL_CSTEP(31) | SYSCTRL_DFLLMUL_FSTEP(511) | SYSCTRL_DFLLMUL_MUL(1465);
	while((REG_SYSCTRL_PCLKSR & (SYSCTRL_PCLKSR_DFLLRDY)) == 0);

	// get factory calibrated value for DFLL_COARSE/FINE from NVM Software Calibration Area
	uint32_t dfllCoarse_calib = *(uint32_t*) FUSES_DFLL48M_COARSE_CAL_ADDR;
	dfllCoarse_calib &= FUSES_DFLL48M_COARSE_CAL_Msk;
	dfllCoarse_calib = dfllCoarse_calib >> FUSES_DFLL48M_COARSE_CAL_Pos;

	uint32_t dfllFine_calib = *(uint32_t*) FUSES_DFLL48M_FINE_CAL_ADDR;
	dfllFine_calib &= FUSES_DFLL48M_FINE_CAL_Msk;
	dfllFine_calib = dfllFine_calib >> FUSES_DFLL48M_FINE_CAL_Pos;

	REG_SYSCTRL_DFLLVAL = dfllCoarse_calib << SYSCTRL_DFLLVAL_COARSE_Pos | dfllFine_calib << SYSCTRL_DFLLVAL_FINE_Pos;
	while((REG_SYSCTRL_PCLKSR & (SYSCTRL_PCLKSR_DFLLRDY)) == 0);

	// enable close loop
	REG_SYSCTRL_DFLLCTRL |= SYSCTRL_DFLLCTRL_MODE | SYSCTRL_DFLLCTRL_WAITLOCK;
	while((REG_SYSCTRL_PCLKSR & (SYSCTRL_PCLKSR_DFLLRDY)) == 0);

	// change CLOCK GENERATOR 0 reference to DFLL48M
	REG_GCLK_GENDIV  = GCLK_GENDIV_DIV(1) | GCLK_GENDIV_ID(0x0);
	REG_GCLK_GENCTRL = GCLK_GENCTRL_RUNSTDBY | GCLK_GENCTRL_IDC | GCLK_GENCTRL_GENEN | GCLK_GENCTRL_SRC_DFLL48M | GCLK_GENCTRL_ID(0x0);
	while(REG_GCLK_STATUS & GCLK_STATUS_SYNCBUSY);
	// Keep the default device state after reset
	SystemCoreClock = __SYSTEM_CLOCK;
	return;
}

/**
 * Update SystemCoreClock variable
 *
 * @brief  Updates the SystemCoreClock with current core Clock
 *         retrieved from cpu registers.
 */
void SystemCoreClockUpdate(void)
{
	// Not implemented
	SystemCoreClock = __SYSTEM_CLOCK;
	return;
}
