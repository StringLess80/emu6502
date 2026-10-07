/*
 * disasm.c - A one-instruction disassembler.
 */
#include <stdio.h>

#include "disasm.h"
#include "opcodes.h"

int disasm(Bus *bus, uint16_t addr, char *out, size_t out_size)
{
    uint8_t opcode = bus_peek(bus, addr);
    const OpInfo *info = &OPCODES[opcode];

    if (info->name == NULL) {
        snprintf(out, out_size, ".BYTE $%02X", opcode);
        return 1;
    }

    uint8_t  lo   = bus_peek(bus, (uint16_t)(addr + 1));
    uint8_t  hi   = bus_peek(bus, (uint16_t)(addr + 2));
    uint16_t word = (uint16_t)(lo | (hi << 8));
    const char *n = info->name;

    switch (info->mode) {
    case MODE_IMP:  snprintf(out, out_size, "%s", n);                 break;
    case MODE_ACC:  snprintf(out, out_size, "%s A", n);               break;
    case MODE_IMM:  snprintf(out, out_size, "%s #$%02X", n, lo);      break;
    case MODE_ZP:   snprintf(out, out_size, "%s $%02X", n, lo);       break;
    case MODE_ZPX:  snprintf(out, out_size, "%s $%02X,X", n, lo);     break;
    case MODE_ZPY:  snprintf(out, out_size, "%s $%02X,Y", n, lo);     break;
    case MODE_ABS:  snprintf(out, out_size, "%s $%04X", n, word);     break;
    case MODE_ABSX: snprintf(out, out_size, "%s $%04X,X", n, word);   break;
    case MODE_ABSY: snprintf(out, out_size, "%s $%04X,Y", n, word);   break;
    case MODE_IND:  snprintf(out, out_size, "%s ($%04X)", n, word);   break;
    case MODE_INDX: snprintf(out, out_size, "%s ($%02X,X)", n, lo);   break;
    case MODE_INDY: snprintf(out, out_size, "%s ($%02X),Y", n, lo);   break;
    case MODE_REL: {
        /* The offset counts from the address after the instruction. */
        uint16_t target = (uint16_t)(addr + 2 + (int8_t)lo);
        snprintf(out, out_size, "%s $%04X", n, target);
        break;
    }
    }
    return mode_length(info->mode);
}
