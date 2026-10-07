/*
 * flatbus.c - The simplest possible bus: 64 KiB of RAM and nothing else.
 *
 * The tests link the CPU against this file instead of sbc.c. Because the
 * CPU only knows the functions declared in bus.h, it cannot tell the
 * difference.
 */
#include "flatbus.h"

uint8_t bus_read(Bus *bus, uint16_t addr)
{
    return bus->mem[addr];
}

void bus_write(Bus *bus, uint16_t addr, uint8_t value)
{
    bus->mem[addr] = value;
}

uint8_t bus_peek(Bus *bus, uint16_t addr)
{
    return bus->mem[addr];
}
