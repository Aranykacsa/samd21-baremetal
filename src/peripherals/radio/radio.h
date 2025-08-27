#ifndef RADIO_H
#define RADIO_H

#include "variables.h"
#include <stdbool.h>

/**
 * Sets up the LoRa radio module with high-performance settings if `boost` is true.
 * Sends configuration commands via Serial1 and checks for "ok" responses.
 */
uint8_t setupRadio(bool boost);

/**
 * Sends a formatted message string over the LoRa radio using Serial1.
 *
 * Appends `" 1\r\n"` to the message and sends it via `radio tx`.
 * Also prints the message to the Serial monitor for debugging purposes.
 *
 * @param msg A pointer to a null-terminated string to be transmitted.
 */
/**
 * Sends a formatted message string over the LoRa radio using Serial1.
 * Ensures even-length hex payloads and waits for radio_tx_ok.
 */
void sendRadio(const char* msg);

#endif /* RADIO_H */