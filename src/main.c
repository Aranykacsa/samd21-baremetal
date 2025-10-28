/*#include "samd21.h"
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include "system_samd21.h"
#include "uart.h"
#include "clock.h"
#include "i2c-master.h"
#include "i2c-helper.h"
#include "clock.h"
#include "variables.h"
#include "board.h"
#include "storage.h"
#include "radio.h"*/

/*float t = 0, h = 0;
char tx_cmd[64];

int main(void) {
  SystemInit();                               // 48 MHz clocks
  SysTick_Config(SystemCoreClock / 1000);

  setup_board();
  setup_peripherals();

  uint32_t next = get_uptime();

  while (1) {
    if ((int32_t)(get_uptime() - next) >= 0) {
        next += 1000;
        measure_env(&t, &h);

        snprintf(tx_cmd, sizeof(tx_cmd), "%.2fb%.2f0", (double)t, (double)h);
        for (size_t i = 0; i < sizeof(tx_cmd); i++) {
          if (tx_cmd[i] == '.') tx_cmd[i] = 'a';
        }

        printf("[RADIO] MSG: %s\r\n",tx_cmd);

        // --- send it ---
        radio_message(tx_cmd);
    }
  }
}*/

#include "samd21.h"

void SERCOM2_USART_init(void)
{
    // === Enable the APBC clock for SERCOM2 ===
    PM->APBCMASK.reg |= PM_APBCMASK_SERCOM2;

    // === Configure the Generic Clock for SERCOM2 core ===
    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM2_GCLK_ID_CORE) |
                        GCLK_CLKCTRL_CLKEN |
                        GCLK_CLKCTRL_GEN(0);   // Use GCLK0
    while (GCLK->STATUS.bit.SYNCBUSY);

    // === Configure PA08 as TX (PAD0) ===
    PORT->Group[0].PINCFG[8].bit.PMUXEN = 1;
    PORT->Group[0].PMUX[8 >> 1].reg &= ~PORT_PMUX_PMUXE_Msk;
    PORT->Group[0].PMUX[8 >> 1].reg |= PORT_PMUX_PMUXE_D;   // Function D = SERCOM2

    // === Reset the SERCOM2 USART ===
    SERCOM2->USART.CTRLA.bit.SWRST = 1;
    while (SERCOM2->USART.CTRLA.bit.SWRST || SERCOM2->USART.SYNCBUSY.bit.SWRST);

    // === Configure CTRLA ===
    SERCOM2->USART.CTRLA.reg = SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
                               SERCOM_USART_CTRLA_DORD |
                               SERCOM_USART_CTRLA_TXPO(0) |   // PAD0 = TX
                               SERCOM_USART_CTRLA_SAMPR(0);

    // === Configure CTRLB ===
    SERCOM2->USART.CTRLB.reg = SERCOM_USART_CTRLB_CHSIZE(0) | // 8-bit
                               SERCOM_USART_CTRLB_TXEN;
    while (SERCOM2->USART.SYNCBUSY.bit.CTRLB);

    // === Baud rate for 115200 (assuming 8 MHz clock) ===
    SERCOM2->USART.BAUD.reg = 50436; // from datasheet table

    // === Enable USART ===
    SERCOM2->USART.CTRLA.bit.ENABLE = 1;
    while (SERCOM2->USART.SYNCBUSY.bit.ENABLE);
}

void SERCOM2_USART_write(char data)
{
    while (!(SERCOM2->USART.INTFLAG.bit.DRE)); // Wait until Data Register Empty
    SERCOM2->USART.DATA.reg = data;
}

int main(void)
{
    SystemInit();
    SERCOM2_USART_init();

    while (1)
    {
        SERCOM2_USART_write('H');
        SERCOM2_USART_write('e');
        SERCOM2_USART_write('l');
        SERCOM2_USART_write('l');
        SERCOM2_USART_write('o');
        SERCOM2_USART_write('\r');
        SERCOM2_USART_write('\n');
        for (volatile uint32_t i = 0; i < 500000; i++); // crude delay
    }
}
