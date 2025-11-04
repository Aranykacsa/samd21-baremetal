// src/drivers/uart/uart.c
#include "samd21.h"
#include "uart.h"
#include "clock.h"
#include "variables.h"

uint16_t calculate_baud_value(uint32_t baudrate, uint32_t clock_hz) {
    return (uint16_t)(65536.0 * (1.0 - ((16.0 * baudrate) / (double)clock_hz)));
}

// === Configure PA22/PA23 for SERCOM3 (MUX C) ===
void uart_pinmux(uart_t* bus) {
    PORT->Group[bus->tx_port].PINCFG[bus->tx_pin].bit.PMUXEN = 1;
    PORT->Group[bus->rx_port].PINCFG[bus->rx_pin].bit.PMUXEN = 1;

    // Update correct nibble in PMUX register
    if (bus->tx_pin & 1)
        PORT->Group[bus->tx_port].PMUX[bus->tx_pin >> 1].bit.PMUXO = bus->pmux_func;
    else
        PORT->Group[bus->tx_port].PMUX[bus->tx_pin >> 1].bit.PMUXE = bus->pmux_func;

    if (bus->rx_pin & 1)
        PORT->Group[bus->rx_port].PMUX[bus->rx_pin >> 1].bit.PMUXO = bus->pmux_func;
    else
        PORT->Group[bus->rx_port].PMUX[bus->rx_pin >> 1].bit.PMUXE = bus->pmux_func;
}

// === Initialize SERCOM3 as UART (PA22 TX, PA23 RX) ===
void uart_init(uart_t* bus) {
    // Enable APBC clock for this SERCOM
    if (bus->sercom == SERCOM0) PM->APBCMASK.bit.SERCOM0_ = 1;
    if (bus->sercom == SERCOM1) PM->APBCMASK.bit.SERCOM1_ = 1;
    if (bus->sercom == SERCOM2) PM->APBCMASK.bit.SERCOM2_ = 1;
    if (bus->sercom == SERCOM3) PM->APBCMASK.bit.SERCOM3_ = 1;
    if (bus->sercom == SERCOM4) PM->APBCMASK.bit.SERCOM4_ = 1;
    if (bus->sercom == SERCOM5) PM->APBCMASK.bit.SERCOM5_ = 1;

    // Connect GCLK0 (48 MHz) to SERCOM core
    uint8_t sercom_id = 0;
    if (bus->sercom == SERCOM0) sercom_id = SERCOM0_GCLK_ID_CORE;
    if (bus->sercom == SERCOM1) sercom_id = SERCOM1_GCLK_ID_CORE;
    if (bus->sercom == SERCOM2) sercom_id = SERCOM2_GCLK_ID_CORE;
    if (bus->sercom == SERCOM3) sercom_id = SERCOM3_GCLK_ID_CORE;
    if (bus->sercom == SERCOM4) sercom_id = SERCOM4_GCLK_ID_CORE;
    if (bus->sercom == SERCOM5) sercom_id = SERCOM5_GCLK_ID_CORE;

    GCLK->CLKCTRL.reg = GCLK_CLKCTRL_ID(sercom_id) |
                        GCLK_CLKCTRL_GEN_GCLK0 |
                        GCLK_CLKCTRL_CLKEN;
    while (GCLK->STATUS.bit.SYNCBUSY);

    // Configure pins
    uart_pinmux(bus);

    // Reset
    bus->sercom->USART.CTRLA.bit.SWRST = 1;
    while (bus->sercom->USART.SYNCBUSY.bit.SWRST);

    // Configure USART mode
    bus->sercom->USART.CTRLA.reg =
        SERCOM_USART_CTRLA_MODE_USART_INT_CLK |
        SERCOM_USART_CTRLA_DORD |
        SERCOM_USART_CTRLA_RXPO(bus->rxpo) |
        SERCOM_USART_CTRLA_TXPO(bus->txpo) |
        SERCOM_USART_CTRLA_SAMPR(0); // 16x oversampling

    // Baud rate
    bus->sercom->USART.BAUD.reg = calculate_baud_value(bus->baud, 48000000);

    // Enable TX/RX
    bus->sercom->USART.CTRLB.reg = SERCOM_USART_CTRLB_TXEN | SERCOM_USART_CTRLB_RXEN;
    while (bus->sercom->USART.SYNCBUSY.bit.CTRLB);

    // Enable USART
    bus->sercom->USART.CTRLA.bit.ENABLE = 1;
    while (bus->sercom->USART.SYNCBUSY.bit.ENABLE);
}

// === Transmit one byte ===
void uart_write(uart_t* bus, uint8_t data) {
    while (!bus->sercom->USART.INTFLAG.bit.DRE);
    bus->sercom->USART.DATA.reg = data;
}

// === Receive one byte (blocking) ===
uint8_t uart_read(uart_t* bus) {
    while (!bus->sercom->USART.INTFLAG.bit.RXC);
    return bus->sercom->USART.DATA.reg;
}

// === Send a string ===
void uart_send_string(uart_t* bus, const char* s) {
    while (*s) uart_write(bus, *s++);
}
