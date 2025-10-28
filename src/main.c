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
    #include "system_samd21.h"
    #include <stdint.h>
    #include <stdio.h>
    #include "clock.h"

    #define USART_BAUD_RATE 115200
    #define USART_SAMPLE_NUM 16

    // === Calculate baud register value ===
    static uint16_t calculate_baud_value(uint32_t baudrate, uint32_t clock_hz) {
        // Formula from AT11626
        uint64_t ratio = ((uint64_t)USART_SAMPLE_NUM * baudrate) << 32;
        uint64_t scale = ((1ULL << 32) - (ratio / clock_hz));
        uint16_t baud = (uint16_t)((65536ULL * scale) >> 32);
        return baud;
    }

    // === Configure PA22/PA23 for SERCOM3 (MUX C) ===
    static void uart_pinmux_sercom3(void) {
        // Enable PMUX on PA22 (even) and PA23 (odd)
        PORT->Group[0].PINCFG[22].bit.PMUXEN = 1;
        PORT->Group[0].PINCFG[23].bit.PMUXEN = 1;

        // MUX C = SERCOM3
        PORT->Group[0].PMUX[22 >> 1].reg = PORT_PMUX_PMUXE_C | PORT_PMUX_PMUXO_C;
    }

    // === Initialize SERCOM3 as UART (PA22 TX, PA23 RX) ===
    static void uart_init_sercom3(void) {
        // Enable APBC clock for SERCOM3
        PM->APBCMASK.bit.SERCOM3_ = 1;

        // Connect GCLK0 (48 MHz) to SERCOM3 core
        GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(SERCOM3_GCLK_ID_CORE) |
                            GCLK_CLKCTRL_GEN_GCLK0 |
                            GCLK_CLKCTRL_CLKEN;
        while (GCLK->STATUS.bit.SYNCBUSY);

        // Configure pins
        uart_pinmux_sercom3();

        // Calculate baud value for 9600 bps @ 48 MHz
        uint16_t baud_val = calculate_baud_value(USART_BAUD_RATE, 48000000);

        // Software reset
        SERCOM3->USART.CTRLA.bit.SWRST = 1;
        while (SERCOM3->USART.SYNCBUSY.bit.SWRST);

        // Configure USART mode
        SERCOM3->USART.CTRLA.reg =
            SERCOM_USART_CTRLA_MODE_USART_INT_CLK |   // Internal clock
            SERCOM_USART_CTRLA_DORD |                 // LSB first
            SERCOM_USART_CTRLA_RXPO(1) |              // RX on PAD1 (PA23)
            SERCOM_USART_CTRLA_TXPO(0) |              // TX on PAD0 (PA22)
            SERCOM_USART_CTRLA_SAMPR(0);              // 16x oversampling

        // Baud rate
        SERCOM3->USART.BAUD.reg = baud_val;

        // Enable TX/RX
        SERCOM3->USART.CTRLB.reg =
            SERCOM_USART_CTRLB_TXEN |
            SERCOM_USART_CTRLB_RXEN;
        while (SERCOM3->USART.SYNCBUSY.bit.CTRLB);

        // Enable USART
        SERCOM3->USART.CTRLA.bit.ENABLE = 1;
        while (SERCOM3->USART.SYNCBUSY.bit.ENABLE);
    }

    // === Transmit one byte ===
    static void uart_write_sercom3(uint8_t data) {
        while (!SERCOM3->USART.INTFLAG.bit.DRE);
        SERCOM3->USART.DATA.reg = data;
    }

    // === Receive one byte (blocking) ===
    static uint8_t uart_read_sercom3(void) {
        while (!SERCOM3->USART.INTFLAG.bit.RXC);
        return SERCOM3->USART.DATA.reg;
    }

    // === Send a string ===
    static void uart_send_string(const char *s) {
        while (*s) uart_write_sercom3(*s++);
    }

    // === Main ===
    int main(void) {
        SystemInit();
        SysTick_Config(SystemCoreClock / 1000);
        uart_init_sercom3();

        char buffer[] = " Hello, World!";
        int index = 0;
        //uart_send_string("SERCOM3 UART ready on PA22/PA23\r\n");

        while (index < sizeof(buffer) - 1) {
            uart_write_sercom3(buffer[index++]);
            uint8_t c = uart_read_sercom3();   // wait for a byte
            printf("Received: %c\r\n", c);
            delay_ms(100);
        }
    }
