/*
 * disasm.h - Turn machine code back into assembly text.
 */
#ifndef DISASM_H
#define DISASM_H

#include <stddef.h>
#include <stdint.h>

#include "bus.h"

/* Disassemble the instruction at addr into out (e.g. "LDA $1001").
 * Returns the length of the instruction in bytes (1 to 3).
 * Uses bus_peek, so it never disturbs the hardware. */
int disasm(Bus *bus, uint16_t addr, char *out, size_t out_size);

#endif /* DISASM_H */
