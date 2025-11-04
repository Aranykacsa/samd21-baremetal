#include "radio.h"
#include "variables.h"
#include "clock.h"
#include "uart.h"
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/*static bool is_terminal_resp(const char* s) {
  // case-insensitive check for keywords
  char low[128];
  size_t n = 0;
  while (s[n] && n < sizeof(low)-1) { low[n] = (char)tolower((unsigned char)s[n]); n++; }
  low[n] = '\0';
  return strstr(low, "ok") || strstr(low, "invalid_param") ||
         strstr(low, "busy") || strstr(low, "error");
}

uint8_t setup_radio(bool boost) {
  printf("Starting radio setup...\r\n");

  // Optional wake nudge; your uart_write_string() already flushes before sending.
  uart_send_string(&uart_s2, "\r\n");
  //uart_drain_tx(&uart_s2);

  if (!boost) {
    printf("Low performance mode not implemented.\r\n");
    return 3;
  }
  printf("Using high performance configuration\r\n");

  char buf[128];
  char cmd[96];

  // Optional probe
  uart_send_string(&uart_s2, "sys get ver\r\n");
  //uart_drain_tx(&uart_s2);
  //size_t n = uart_readline(&uart_s2, buf, sizeof(buf), 800, 300);
  //if (n) printf("[RADIO] ver: %s\r\n", buf); else printf("[RADIO] ver: timeout\r\n");

  for (size_t i = 0; i < radio_commands_len; ++i) {
    snprintf(cmd, sizeof(cmd), "radio set %s\r\n", radio_commands[i]);
    printf("[RADIO] SEND: %s", cmd);

    uart_send_string(&uart_s2, cmd);   // (auto-flush before send)
    //uart_drain_tx(&uart_s2);            // wait until fully sent

    bool terminal = false;
    // read up to 3 lines per command (echo + status), bounded time
    for (int line = 0; line < 3; ++line) {
      //n = uart_readline(&uart_s2, buf, sizeof(buf), 1200, 400);
      //if (n == 0) { printf("[RADIO] timeout on cmd %zu\r\n", i); break; }
      printf("[RADIO] RESP: %s\r\n", buf);
      if (is_terminal_resp(buf)) { terminal = true; break; }
      // else likely echo; continue reading the next line
    }

    if (!terminal) printf("[RADIO] no terminal resp for cmd %zu\r\n", i);
  }

  printf("Radio setup complete.\r\n");
  return 0;
}

// $GPRMC,110618.00,A,4112.34567,N,00831.54753,W,0.078,,030118,,,A*6A
// 110618c00bAb4112c34567b00831c54753

void radio_message(const char* msg) {
  if (strlen(msg) % 2 != 0) {
    printf("Error: Payload length not even: %s\r\n", msg);
  }
  char cmd[96];
  snprintf(cmd, sizeof(cmd), "radio tx %s 1\r\n", msg);

  uart_send_string(&uart_s2, cmd); // flush-before-send happens inside
  //uart_drain_tx(&uart_s2);

  char buf[128];
  //size_t n = uart_readline(&uart_s2, buf, sizeof(buf), 2000, 500);
  //if (n) printf("[RADIO] TX resp: %s\r\n", buf);
  else   printf("[RADIO] TX resp: timeout\r\n");
}*/
