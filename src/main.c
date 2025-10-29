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
    #include "uart.h"
    #include "clock.h"
#include "variables.h"

    // === Main ===
    int main(void) {
        SystemInit();
        SysTick_Config(SystemCoreClock / 1000);
        uart_init(&uart_s2);

        char buffer[] = " Hello, World!";
        int index = 0;
        //uart_send_string("SERCOM3 UART ready on PA22/PA23\r\n");

        while (index < sizeof(buffer) - 1) {
            uart_write(&uart_s2, buffer[index++]);
            uint8_t c = uart_read(&uart_s2);   // wait for a byte
            printf("Received: %c\r\n", c);
            delay_ms(100);
        }
    }
