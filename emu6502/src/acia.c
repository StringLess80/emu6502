/*
 * acia.c - 6551 ACIA emulation.
 *
 * We do not emulate baud rates: a byte written to DATA is "sent"
 * instantly, so the transmitter is always ready (TDRE is always 1).
 * On the receive side we keep a single byte, like the real chip.
 */
#include "acia.h"

void acia_reset(Acia *acia)
{
    acia->rx_data = 0;
    acia->rx_full = false;
    acia->tx_data = 0;
    acia->tx_full = false;
    acia->command = 0x02;   /* receiver IRQ disabled, as after reset */
    acia->control = 0x00;
}

static uint8_t status(const Acia *acia)
{
    uint8_t s = ACIA_STATUS_TDRE;          /* we can always transmit */
    if (acia->rx_full) {
        s |= ACIA_STATUS_RDRF;
    }
    return s;
}

uint8_t acia_peek(const Acia *acia, uint8_t reg)
{
    switch (reg & 0x03) {
    case 0:  return acia->rx_data;
    case 1:  return status(acia);
    case 2:  return acia->command;
    default: return acia->control;
    }
}

uint8_t acia_read(Acia *acia, uint8_t reg)
{
    uint8_t value = acia_peek(acia, reg);
    if ((reg & 0x03) == 0) {
        acia->rx_full = false;   /* reading DATA empties the receiver */
    }
    return value;
}

void acia_write(Acia *acia, uint8_t reg, uint8_t value)
{
    switch (reg & 0x03) {
    case 0:                                   /* DATA: transmit */
        acia->tx_data = value;
        acia->tx_full = true;
        break;
    case 1:                                   /* programmed reset */
        acia->command &= 0xE0;
        acia->rx_full = false;
        break;
    case 2:
        acia->command = value;
        break;
    default:
        acia->control = value;
        break;
    }
}

bool acia_receive(Acia *acia, uint8_t byte)
{
    if (acia->rx_full) {
        return false;            /* program has not read the last one */
    }
    acia->rx_data = byte;
    acia->rx_full = true;
    return true;
}

bool acia_transmit(Acia *acia, uint8_t *byte)
{
    if (!acia->tx_full) {
        return false;
    }
    *byte = acia->tx_data;
    acia->tx_full = false;
    return true;
}
