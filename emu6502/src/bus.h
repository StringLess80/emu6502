/*
 * bus.h - What the CPU is allowed to know about the outside world.
 *
 * The CPU only ever does three things with the rest of the computer:
 * it reads a byte, it writes a byte, and (for the disassembler) it
 * peeks at a byte without disturbing anything. That is the whole
 * interface.
 *
 * "struct Bus" is deliberately left incomplete here (an "opaque type").
 * The real SBC defines it in sbc.h; the test programs define their own
 * much simpler version. The CPU code works with either, because it only
 * ever handles a pointer to a Bus.
 */
#ifndef BUS_H
#define BUS_H

#include <stdint.h>

typedef struct Bus Bus;

/* A normal read, exactly like the CPU does it. May have side effects
 * (reading the ACIA data register, for example, empties it). */
uint8_t bus_read(Bus *bus, uint16_t addr);

/* A normal write. Writes to ROM are silently ignored. */
void bus_write(Bus *bus, uint16_t addr, uint8_t value);

/* A read with no side effects, for debugging tools. */
uint8_t bus_peek(Bus *bus, uint16_t addr);

#endif /* BUS_H */
