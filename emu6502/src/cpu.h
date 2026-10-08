/*
 * cpu.h - The 6502 processor core.
 */
#ifndef CPU_H
#define CPU_H

#include <stdbool.h>
#include <stdint.h>

#include "bus.h"

/* Bits of the processor status register P. */
enum {
    FLAG_C = 0x01,  /* Carry                                  */
    FLAG_Z = 0x02,  /* Zero                                   */
    FLAG_I = 0x04,  /* Interrupt disable                      */
    FLAG_D = 0x08,  /* Decimal mode                           */
    FLAG_B = 0x10,  /* Break (only exists in the pushed copy) */
    FLAG_U = 0x20,  /* Unused - always 1                      */
    FLAG_V = 0x40,  /* Overflow                               */
    FLAG_N = 0x80   /* Negative                               */
};

typedef struct {
    uint8_t  a;        /* accumulator                         */
    uint8_t  x;        /* index register X                    */
    uint8_t  y;        /* index register Y                    */
    uint8_t  sp;       /* stack pointer (stack is $0100-$01FF) */
    uint8_t  p;        /* processor status (flags)            */
    uint16_t pc;       /* program counter                     */

    uint64_t cycles;   /* clock cycles since power-on         */
    bool     halted;   /* true after a JAM opcode (until reset) */
    Bus     *bus;      /* where memory and I/O live           */
} Cpu;

/* Connect the CPU to a bus and clear all registers. */
void cpu_init(Cpu *cpu, Bus *bus);

/* Do what the real chip does when RESB goes low then high. */
void cpu_reset(Cpu *cpu);

/* Execute exactly one instruction. Returns the cycles it took. */
int cpu_step(Cpu *cpu);

#endif /* CPU_H */
