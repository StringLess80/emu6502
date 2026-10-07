/*
 * acia.h - The 6551 ACIA (serial port) of the SBC.
 *
 * Four registers, selected by address lines A1 and A0:
 *   0  DATA     read: received byte    write: byte to transmit
 *   1  STATUS   read: status flags     write: "programmed reset"
 *   2  COMMAND  parity, echo, interrupts, DTR
 *   3  CONTROL  baud rate, word length, stop bits
 */
#ifndef ACIA_H
#define ACIA_H

#include <stdbool.h>
#include <stdint.h>

/* Status register bits that the BIOS and WozMon look at. */
enum {
    ACIA_STATUS_RDRF = 0x08,   /* Receive Data Register Full  */
    ACIA_STATUS_TDRE = 0x10    /* Transmit Data Register Empty */
};

typedef struct {
    uint8_t rx_data;    /* last byte received from the terminal   */
    bool    rx_full;    /* is rx_data waiting to be read?         */
    uint8_t tx_data;    /* last byte the program sent             */
    bool    tx_full;    /* is tx_data waiting to go to the screen? */
    uint8_t command;
    uint8_t control;
} Acia;

void    acia_reset(Acia *acia);
uint8_t acia_read(Acia *acia, uint8_t reg);
uint8_t acia_peek(const Acia *acia, uint8_t reg);
void    acia_write(Acia *acia, uint8_t reg, uint8_t value);

/* Host side: the emulator's main loop uses these two. */
bool acia_receive(Acia *acia, uint8_t byte);     /* key pressed      */
bool acia_transmit(Acia *acia, uint8_t *byte);   /* byte for screen? */

#endif /* ACIA_H */
