#include "radio.h"
#include "variables.h"
#include "clock.h"

uint8_t setupRadio(bool boost) {
  printf("Starting radio setup...");


  //uart_init()
  Serial1.begin(115200);

  delay_ms(3000); 

  if (boost) {
    printf("Using high performance configuration");

    size_t setLen = sizeof(setCommands) / sizeof(setCommands[0]);
    for (size_t i = 0; i < setLen; ++i) {
      
      //radio uart
      Serial1.printf("radio set %s\r\n", setCommands[i]);

      char response[64];
      unsigned long start = millis();
      while (millis() - start < 1000) {
        while (Serial1.available()) {
          char c = Serial1.read();
          printf("%s\n", c);      
          response += c;
          if (c == '\n') break;  
        }
        if (response.endsWith("\n")) break;
      }

      //egész biztos, hogy c-ben ilyen nincs
      response.trim();

      if (response != "ok") {
        printf("Command failed: ");
        printf("%s\r\n", radio_commands[i]);
        printf("Response: ");
        printf("\r\n", response);
      }
    }
  } else {
    printf("Low performance mode not implemented.");
    return 3;
  }

  Serial.println("Radio setup complete.");
  return 0;
}

void sendRadio(const char* msg) {
  if (strlen(msg) % 2 != 0) {
    printf("Error: Payload length not even: ");
    printf("%s\r\n", );(msg);
    return;
  }

  Serial1.printf("radio tx %s 1\r\n", msg);
}