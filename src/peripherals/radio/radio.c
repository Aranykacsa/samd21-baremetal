#include "radio.h"
#include "variables.h"
#include "clock.h"
#include "uart.h"
#include <stdbool.h>
#include <string.h>           
#include <stdio.h>
#include <ctype.h>

uint8_t setup_radio(bool boost) {
  printf("Starting radio setup...");

  uart_write_string(&uart_s2, "\r\n");


  if (boost) {
    printf("Using high performance configuration");

    char buf[64];
    char cmd[64];
    for (size_t i = 0; i < radio_commands_len; ++i) {
      
      snprintf(cmd, sizeof(cmd), "radio set %s\r\n", radio_commands[i]);      
      uart_write_string(&uart_s2, cmd);

      size_t n = uart_read_string_blocking(&uart_s2, buf, 64, 1000);
      if (n > 0) {
          printf("Loopback: %s\n", buf);   
      } else {
          printf("No loopback!\n");
      }
    }
  } else {
    printf("Low performance mode not implemented.");
    return 3;
  }

  printf("Radio setup complete.");
  return 0;
}

void radio_message(const char* msg) {
  if (strlen(msg) % 2 != 0) {
    printf("Error: Payload length not even: ");
    printf("%s\r\n", msg);
    return;
  }

  char cmd[64];
  snprintf(cmd, sizeof(cmd), "radio tx %s\r\n", msg);      
  uart_write_string(&uart_s2, cmd);
}