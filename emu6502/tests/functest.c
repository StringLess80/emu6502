/*
 * functest.c - Run Klaus Dormann's 6502 functional test.
 *
 * The test is a 64 KiB memory image. It starts at $0400 and tests every
 * official instruction, every addressing mode and every flag. When a test
 * fails, the program jumps to itself forever ("jmp *") at the place of the
 * failure. When everything passes it does the same at the success address.
 * So: we run until PC stops changing, then look at where we are.
 *
 * Usage: ./functest 6502_functional_test.bin
 */
#include <stdio.h>
#include <stdlib.h>

#include "../src/cpu.h"
#include "flatbus.h"

#define START_ADDR   0x0400
#define SUCCESS_ADDR 0x3469

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "usage: %s 6502_functional_test.bin\n", argv[0]);
        return EXIT_FAILURE;
    }

    /* 64 KiB is too much to put on the stack comfortably, so we ask
     * for it with malloc (see the chapter on dynamic memory). */
    Bus *bus = malloc(sizeof *bus);
    if (bus == NULL) {
        fprintf(stderr, "out of memory\n");
        return EXIT_FAILURE;
    }

    FILE *f = fopen(argv[1], "rb");
    if (f == NULL) {
        perror(argv[1]);
        free(bus);
        return EXIT_FAILURE;
    }
    size_t n = fread(bus->mem, 1, sizeof bus->mem, f);
    fclose(f);
    if (n != sizeof bus->mem) {
        fprintf(stderr, "%s: expected 65536 bytes, got %zu\n", argv[1], n);
        free(bus);
        return EXIT_FAILURE;
    }

    Cpu cpu;
    cpu_init(&cpu, bus);
    cpu.pc = START_ADDR;
    cpu.sp = 0xFD;

    uint16_t last_pc = 0;
    for (;;) {
        last_pc = cpu.pc;
        cpu_step(&cpu);
        if (cpu.halted) {
            printf("FAIL: illegal opcode $%02X at $%04X\n",
                   bus->mem[cpu.pc], cpu.pc);
            break;
        }
        if (cpu.pc == last_pc) {   /* "jmp *" - we are stuck */
            break;
        }
    }

    int ok = (cpu.pc == SUCCESS_ADDR);
    printf("%s: stopped at $%04X after %llu cycles\n",
           ok ? "PASS" : "FAIL", cpu.pc, (unsigned long long)cpu.cycles);

    free(bus);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
