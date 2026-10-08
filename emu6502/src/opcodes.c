/*
 * opcodes.c - The table of all 256 NMOS 6502 opcodes:
 *               the 151 official ones and the 105 undocumented ones.
 *
 * The syntax [0xA9] = { ... } is a "designated initializer": it fills in
 * element 0xA9 of the array. Every element we do not mention would be
 * filled with zeros (a NULL name), but here all 256 are filled in.
 */
#include "opcodes.h"

const OpInfo OPCODES[256] = {
    /* ADC - add with carry */
    [0x69] = {"ADC", MODE_IMM,  2}, [0x65] = {"ADC", MODE_ZP,   3},
    [0x75] = {"ADC", MODE_ZPX,  4}, [0x6D] = {"ADC", MODE_ABS,  4},
    [0x7D] = {"ADC", MODE_ABSX, 4}, [0x79] = {"ADC", MODE_ABSY, 4},
    [0x61] = {"ADC", MODE_INDX, 6}, [0x71] = {"ADC", MODE_INDY, 5},

    /* AND - bitwise AND with accumulator */
    [0x29] = {"AND", MODE_IMM,  2}, [0x25] = {"AND", MODE_ZP,   3},
    [0x35] = {"AND", MODE_ZPX,  4}, [0x2D] = {"AND", MODE_ABS,  4},
    [0x3D] = {"AND", MODE_ABSX, 4}, [0x39] = {"AND", MODE_ABSY, 4},
    [0x21] = {"AND", MODE_INDX, 6}, [0x31] = {"AND", MODE_INDY, 5},

    /* ASL - arithmetic shift left */
    [0x0A] = {"ASL", MODE_ACC,  2}, [0x06] = {"ASL", MODE_ZP,   5},
    [0x16] = {"ASL", MODE_ZPX,  6}, [0x0E] = {"ASL", MODE_ABS,  6},
    [0x1E] = {"ASL", MODE_ABSX, 7},

    /* Branches */
    [0x90] = {"BCC", MODE_REL,  2}, [0xB0] = {"BCS", MODE_REL,  2},
    [0xF0] = {"BEQ", MODE_REL,  2}, [0x30] = {"BMI", MODE_REL,  2},
    [0xD0] = {"BNE", MODE_REL,  2}, [0x10] = {"BPL", MODE_REL,  2},
    [0x50] = {"BVC", MODE_REL,  2}, [0x70] = {"BVS", MODE_REL,  2},

    /* BIT - test bits */
    [0x24] = {"BIT", MODE_ZP,   3}, [0x2C] = {"BIT", MODE_ABS,  4},

    /* BRK - software interrupt */
    [0x00] = {"BRK", MODE_IMP,  7},

    /* Flag clear / set */
    [0x18] = {"CLC", MODE_IMP,  2}, [0xD8] = {"CLD", MODE_IMP,  2},
    [0x58] = {"CLI", MODE_IMP,  2}, [0xB8] = {"CLV", MODE_IMP,  2},
    [0x38] = {"SEC", MODE_IMP,  2}, [0xF8] = {"SED", MODE_IMP,  2},
    [0x78] = {"SEI", MODE_IMP,  2},

    /* CMP - compare accumulator */
    [0xC9] = {"CMP", MODE_IMM,  2}, [0xC5] = {"CMP", MODE_ZP,   3},
    [0xD5] = {"CMP", MODE_ZPX,  4}, [0xCD] = {"CMP", MODE_ABS,  4},
    [0xDD] = {"CMP", MODE_ABSX, 4}, [0xD9] = {"CMP", MODE_ABSY, 4},
    [0xC1] = {"CMP", MODE_INDX, 6}, [0xD1] = {"CMP", MODE_INDY, 5},

    /* CPX / CPY - compare X / Y */
    [0xE0] = {"CPX", MODE_IMM,  2}, [0xE4] = {"CPX", MODE_ZP,   3},
    [0xEC] = {"CPX", MODE_ABS,  4},
    [0xC0] = {"CPY", MODE_IMM,  2}, [0xC4] = {"CPY", MODE_ZP,   3},
    [0xCC] = {"CPY", MODE_ABS,  4},

    /* DEC / DEX / DEY - decrement */
    [0xC6] = {"DEC", MODE_ZP,   5}, [0xD6] = {"DEC", MODE_ZPX,  6},
    [0xCE] = {"DEC", MODE_ABS,  6}, [0xDE] = {"DEC", MODE_ABSX, 7},
    [0xCA] = {"DEX", MODE_IMP,  2}, [0x88] = {"DEY", MODE_IMP,  2},

    /* EOR - exclusive OR with accumulator */
    [0x49] = {"EOR", MODE_IMM,  2}, [0x45] = {"EOR", MODE_ZP,   3},
    [0x55] = {"EOR", MODE_ZPX,  4}, [0x4D] = {"EOR", MODE_ABS,  4},
    [0x5D] = {"EOR", MODE_ABSX, 4}, [0x59] = {"EOR", MODE_ABSY, 4},
    [0x41] = {"EOR", MODE_INDX, 6}, [0x51] = {"EOR", MODE_INDY, 5},

    /* INC / INX / INY - increment */
    [0xE6] = {"INC", MODE_ZP,   5}, [0xF6] = {"INC", MODE_ZPX,  6},
    [0xEE] = {"INC", MODE_ABS,  6}, [0xFE] = {"INC", MODE_ABSX, 7},
    [0xE8] = {"INX", MODE_IMP,  2}, [0xC8] = {"INY", MODE_IMP,  2},

    /* Jumps and subroutines */
    [0x4C] = {"JMP", MODE_ABS,  3}, [0x6C] = {"JMP", MODE_IND,  5},
    [0x20] = {"JSR", MODE_ABS,  6}, [0x60] = {"RTS", MODE_IMP,  6},
    [0x40] = {"RTI", MODE_IMP,  6},

    /* LDA / LDX / LDY - load */
    [0xA9] = {"LDA", MODE_IMM,  2}, [0xA5] = {"LDA", MODE_ZP,   3},
    [0xB5] = {"LDA", MODE_ZPX,  4}, [0xAD] = {"LDA", MODE_ABS,  4},
    [0xBD] = {"LDA", MODE_ABSX, 4}, [0xB9] = {"LDA", MODE_ABSY, 4},
    [0xA1] = {"LDA", MODE_INDX, 6}, [0xB1] = {"LDA", MODE_INDY, 5},
    [0xA2] = {"LDX", MODE_IMM,  2}, [0xA6] = {"LDX", MODE_ZP,   3},
    [0xB6] = {"LDX", MODE_ZPY,  4}, [0xAE] = {"LDX", MODE_ABS,  4},
    [0xBE] = {"LDX", MODE_ABSY, 4},
    [0xA0] = {"LDY", MODE_IMM,  2}, [0xA4] = {"LDY", MODE_ZP,   3},
    [0xB4] = {"LDY", MODE_ZPX,  4}, [0xAC] = {"LDY", MODE_ABS,  4},
    [0xBC] = {"LDY", MODE_ABSX, 4},

    /* LSR - logical shift right */
    [0x4A] = {"LSR", MODE_ACC,  2}, [0x46] = {"LSR", MODE_ZP,   5},
    [0x56] = {"LSR", MODE_ZPX,  6}, [0x4E] = {"LSR", MODE_ABS,  6},
    [0x5E] = {"LSR", MODE_ABSX, 7},

    /* NOP - no operation */
    [0xEA] = {"NOP", MODE_IMP,  2},

    /* ORA - bitwise OR with accumulator */
    [0x09] = {"ORA", MODE_IMM,  2}, [0x05] = {"ORA", MODE_ZP,   3},
    [0x15] = {"ORA", MODE_ZPX,  4}, [0x0D] = {"ORA", MODE_ABS,  4},
    [0x1D] = {"ORA", MODE_ABSX, 4}, [0x19] = {"ORA", MODE_ABSY, 4},
    [0x01] = {"ORA", MODE_INDX, 6}, [0x11] = {"ORA", MODE_INDY, 5},

    /* Stack */
    [0x48] = {"PHA", MODE_IMP,  3}, [0x08] = {"PHP", MODE_IMP,  3},
    [0x68] = {"PLA", MODE_IMP,  4}, [0x28] = {"PLP", MODE_IMP,  4},

    /* ROL / ROR - rotate */
    [0x2A] = {"ROL", MODE_ACC,  2}, [0x26] = {"ROL", MODE_ZP,   5},
    [0x36] = {"ROL", MODE_ZPX,  6}, [0x2E] = {"ROL", MODE_ABS,  6},
    [0x3E] = {"ROL", MODE_ABSX, 7},
    [0x6A] = {"ROR", MODE_ACC,  2}, [0x66] = {"ROR", MODE_ZP,   5},
    [0x76] = {"ROR", MODE_ZPX,  6}, [0x6E] = {"ROR", MODE_ABS,  6},
    [0x7E] = {"ROR", MODE_ABSX, 7},

    /* SBC - subtract with carry (borrow) */
    [0xE9] = {"SBC", MODE_IMM,  2}, [0xE5] = {"SBC", MODE_ZP,   3},
    [0xF5] = {"SBC", MODE_ZPX,  4}, [0xED] = {"SBC", MODE_ABS,  4},
    [0xFD] = {"SBC", MODE_ABSX, 4}, [0xF9] = {"SBC", MODE_ABSY, 4},
    [0xE1] = {"SBC", MODE_INDX, 6}, [0xF1] = {"SBC", MODE_INDY, 5},

    /* STA / STX / STY - store */
    [0x85] = {"STA", MODE_ZP,   3}, [0x95] = {"STA", MODE_ZPX,  4},
    [0x8D] = {"STA", MODE_ABS,  4}, [0x9D] = {"STA", MODE_ABSX, 5},
    [0x99] = {"STA", MODE_ABSY, 5}, [0x81] = {"STA", MODE_INDX, 6},
    [0x91] = {"STA", MODE_INDY, 6},
    [0x86] = {"STX", MODE_ZP,   3}, [0x96] = {"STX", MODE_ZPY,  4},
    [0x8E] = {"STX", MODE_ABS,  4},
    [0x84] = {"STY", MODE_ZP,   3}, [0x94] = {"STY", MODE_ZPX,  4},
    [0x8C] = {"STY", MODE_ABS,  4},

    /* Register transfers */
    [0xAA] = {"TAX", MODE_IMP,  2}, [0xA8] = {"TAY", MODE_IMP,  2},
    [0xBA] = {"TSX", MODE_IMP,  2}, [0x8A] = {"TXA", MODE_IMP,  2},
    [0x9A] = {"TXS", MODE_IMP,  2}, [0x98] = {"TYA", MODE_IMP,  2},

    /* ============================================================ */
    /* The 105 undocumented opcodes of the NMOS 6502                 */
    /* ============================================================ */

    /* SLO */
    [0x07] = {"SLO", MODE_ZP,   5}, [0x17] = {"SLO", MODE_ZPX,  6},
    [0x0F] = {"SLO", MODE_ABS,  6}, [0x1F] = {"SLO", MODE_ABSX, 7},
    [0x1B] = {"SLO", MODE_ABSY, 7}, [0x03] = {"SLO", MODE_INDX, 8},
    [0x13] = {"SLO", MODE_INDY, 8},
    /* RLA */
    [0x27] = {"RLA", MODE_ZP,   5}, [0x37] = {"RLA", MODE_ZPX,  6},
    [0x2F] = {"RLA", MODE_ABS,  6}, [0x3F] = {"RLA", MODE_ABSX, 7},
    [0x3B] = {"RLA", MODE_ABSY, 7}, [0x23] = {"RLA", MODE_INDX, 8},
    [0x33] = {"RLA", MODE_INDY, 8},
    /* SRE */
    [0x47] = {"SRE", MODE_ZP,   5}, [0x57] = {"SRE", MODE_ZPX,  6},
    [0x4F] = {"SRE", MODE_ABS,  6}, [0x5F] = {"SRE", MODE_ABSX, 7},
    [0x5B] = {"SRE", MODE_ABSY, 7}, [0x43] = {"SRE", MODE_INDX, 8},
    [0x53] = {"SRE", MODE_INDY, 8},
    /* RRA */
    [0x67] = {"RRA", MODE_ZP,   5}, [0x77] = {"RRA", MODE_ZPX,  6},
    [0x6F] = {"RRA", MODE_ABS,  6}, [0x7F] = {"RRA", MODE_ABSX, 7},
    [0x7B] = {"RRA", MODE_ABSY, 7}, [0x63] = {"RRA", MODE_INDX, 8},
    [0x73] = {"RRA", MODE_INDY, 8},
    /* DCP */
    [0xC7] = {"DCP", MODE_ZP,   5}, [0xD7] = {"DCP", MODE_ZPX,  6},
    [0xCF] = {"DCP", MODE_ABS,  6}, [0xDF] = {"DCP", MODE_ABSX, 7},
    [0xDB] = {"DCP", MODE_ABSY, 7}, [0xC3] = {"DCP", MODE_INDX, 8},
    [0xD3] = {"DCP", MODE_INDY, 8},
    /* ISC */
    [0xE7] = {"ISC", MODE_ZP,   5}, [0xF7] = {"ISC", MODE_ZPX,  6},
    [0xEF] = {"ISC", MODE_ABS,  6}, [0xFF] = {"ISC", MODE_ABSX, 7},
    [0xFB] = {"ISC", MODE_ABSY, 7}, [0xE3] = {"ISC", MODE_INDX, 8},
    [0xF3] = {"ISC", MODE_INDY, 8},

    /* LAX - LDA and LDX at once; LXA - unstable LAX # */
    [0xA7] = {"LAX", MODE_ZP,   3}, [0xB7] = {"LAX", MODE_ZPY,  4},
    [0xAF] = {"LAX", MODE_ABS,  4}, [0xBF] = {"LAX", MODE_ABSY, 4},
    [0xA3] = {"LAX", MODE_INDX, 6}, [0xB3] = {"LAX", MODE_INDY, 5},
    [0xAB] = {"LXA", MODE_IMM,  2},

    /* SAX - store A AND X */
    [0x87] = {"SAX", MODE_ZP,   3}, [0x97] = {"SAX", MODE_ZPY,  4},
    [0x8F] = {"SAX", MODE_ABS,  4}, [0x83] = {"SAX", MODE_INDX, 6},

    /* Immediate combinations */
    [0x0B] = {"ANC", MODE_IMM,  2}, [0x2B] = {"ANC", MODE_IMM,  2},
    [0x4B] = {"ALR", MODE_IMM,  2}, [0x6B] = {"ARR", MODE_IMM,  2},
    [0xCB] = {"SBX", MODE_IMM,  2}, [0xEB] = {"SBC", MODE_IMM,  2},
    [0x8B] = {"ANE", MODE_IMM,  2},

    /* Stores ANDed with the address high byte, and LAS */
    [0x93] = {"SHA", MODE_INDY, 6}, [0x9F] = {"SHA", MODE_ABSY, 5},
    [0x9E] = {"SHX", MODE_ABSY, 5}, [0x9C] = {"SHY", MODE_ABSX, 5},
    [0x9B] = {"TAS", MODE_ABSY, 5}, [0xBB] = {"LAS", MODE_ABSY, 4},

    /* NOPs of every size */
    [0x1A] = {"NOP", MODE_IMP,  2}, [0x3A] = {"NOP", MODE_IMP,  2},
    [0x5A] = {"NOP", MODE_IMP,  2}, [0x7A] = {"NOP", MODE_IMP,  2},
    [0xDA] = {"NOP", MODE_IMP,  2}, [0xFA] = {"NOP", MODE_IMP,  2},
    [0x80] = {"NOP", MODE_IMM,  2}, [0x82] = {"NOP", MODE_IMM,  2},
    [0x89] = {"NOP", MODE_IMM,  2}, [0xC2] = {"NOP", MODE_IMM,  2},
    [0xE2] = {"NOP", MODE_IMM,  2}, [0x04] = {"NOP", MODE_ZP,   3},
    [0x44] = {"NOP", MODE_ZP,   3}, [0x64] = {"NOP", MODE_ZP,   3},
    [0x14] = {"NOP", MODE_ZPX,  4}, [0x34] = {"NOP", MODE_ZPX,  4},
    [0x54] = {"NOP", MODE_ZPX,  4}, [0x74] = {"NOP", MODE_ZPX,  4},
    [0xD4] = {"NOP", MODE_ZPX,  4}, [0xF4] = {"NOP", MODE_ZPX,  4},
    [0x0C] = {"NOP", MODE_ABS,  4}, [0x1C] = {"NOP", MODE_ABSX, 4},
    [0x3C] = {"NOP", MODE_ABSX, 4}, [0x5C] = {"NOP", MODE_ABSX, 4},
    [0x7C] = {"NOP", MODE_ABSX, 4}, [0xDC] = {"NOP", MODE_ABSX, 4},
    [0xFC] = {"NOP", MODE_ABSX, 4},

    /* JAM - locks the CPU up until reset */
    [0x02] = {"JAM", MODE_IMP,  2}, [0x12] = {"JAM", MODE_IMP,  2},
    [0x22] = {"JAM", MODE_IMP,  2}, [0x32] = {"JAM", MODE_IMP,  2},
    [0x42] = {"JAM", MODE_IMP,  2}, [0x52] = {"JAM", MODE_IMP,  2},
    [0x62] = {"JAM", MODE_IMP,  2}, [0x72] = {"JAM", MODE_IMP,  2},
    [0x92] = {"JAM", MODE_IMP,  2}, [0xB2] = {"JAM", MODE_IMP,  2},
    [0xD2] = {"JAM", MODE_IMP,  2}, [0xF2] = {"JAM", MODE_IMP,  2},
};

int mode_length(AddrMode mode)
{
    switch (mode) {
    case MODE_IMP:
    case MODE_ACC:
        return 1;
    case MODE_ABS:
    case MODE_ABSX:
    case MODE_ABSY:
    case MODE_IND:
        return 3;
    default:
        return 2;   /* IMM, ZP, ZPX, ZPY, INDX, INDY, REL */
    }
}
