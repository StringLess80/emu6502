/*
 * opcodes.h - The table that describes every 6502 instruction.
 *
 * Both the CPU (for cycle counts) and the disassembler (for names and
 * addressing modes) read this table, so the information lives in exactly
 * one place.
 */
#ifndef OPCODES_H
#define OPCODES_H

#include <stdint.h>

/* The 13 ways a 6502 instruction can find its operand. */
typedef enum {
    MODE_IMP,   /* implied:        CLC                         */
    MODE_ACC,   /* accumulator:    ASL A                       */
    MODE_IMM,   /* immediate:      LDA #$12                    */
    MODE_ZP,    /* zero page:      LDA $12                     */
    MODE_ZPX,   /* zero page,X:    LDA $12,X                   */
    MODE_ZPY,   /* zero page,Y:    LDX $12,Y                   */
    MODE_ABS,   /* absolute:       LDA $1234                   */
    MODE_ABSX,  /* absolute,X:     LDA $1234,X                 */
    MODE_ABSY,  /* absolute,Y:     LDA $1234,Y                 */
    MODE_IND,   /* indirect:       JMP ($1234)                 */
    MODE_INDX,  /* (indirect,X):   LDA ($12,X)                 */
    MODE_INDY,  /* (indirect),Y:   LDA ($12),Y                 */
    MODE_REL    /* relative:       BNE label                   */
} AddrMode;

typedef struct {
    const char *name;    /* "LDA", "LAX", "JAM"...            */
    AddrMode    mode;
    uint8_t     cycles;  /* base number of clock cycles          */
} OpInfo;

/* One entry for each of the 256 possible opcode bytes. */
extern const OpInfo OPCODES[256];

/* How many bytes an instruction in this mode occupies (1, 2 or 3). */
int mode_length(AddrMode mode);

#endif /* OPCODES_H */
