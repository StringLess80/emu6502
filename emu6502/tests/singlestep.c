/*
 * singlestep.c - Runner for the SingleStepTests 6502 test vectors.
 *
 * https://github.com/SingleStepTests/65x02 has 10,000 randomised tests
 * for every one of the 256 opcodes, recorded from a real NMOS 6502.
 * Each one gives the registers and memory before a single instruction,
 * and what they must be afterwards, plus the number of cycles it took.
 *
 * Parsing JSON in C would be a project of its own, so singlestep.py
 * turns each test into one line of plain numbers and pipes them here:
 *
 *   name  pc s a x y p n (addr value)*n   pc s a x y p n (addr value)*n  cycles
 *
 * The first group is the state before, the second the state after.
 */
#include <stdio.h>
#include <string.h>

#include "../src/cpu.h"
#include "flatbus.h"

#define MAX_RAM 64

typedef struct {
    unsigned pc, s, a, x, y, p;
    unsigned count;
    unsigned addr[MAX_RAM], value[MAX_RAM];
} State;

static Bus bus;

static int read_state(State *st)
{
    if (scanf("%u %u %u %u %u %u %u", &st->pc, &st->s, &st->a, &st->x,
              &st->y, &st->p, &st->count) != 7 || st->count > MAX_RAM) {
        return 0;
    }
    for (unsigned i = 0; i < st->count; i++) {
        if (scanf("%u %u", &st->addr[i], &st->value[i]) != 2) {
            return 0;
        }
    }
    return 1;
}

int main(void)
{
    char name[64];
    State before, after;
    unsigned cycles;
    long total = 0, failed = 0;

    while (scanf("%63s", name) == 1) {
        if (!read_state(&before) || !read_state(&after)
            || scanf("%u", &cycles) != 1) {
            fprintf(stderr, "bad input in test %s\n", name);
            return 2;
        }

        Cpu cpu;
        cpu_init(&cpu, &bus);
        cpu.pc = (uint16_t)before.pc;
        cpu.sp = (uint8_t)before.s;
        cpu.a  = (uint8_t)before.a;
        cpu.x  = (uint8_t)before.x;
        cpu.y  = (uint8_t)before.y;
        cpu.p  = (uint8_t)before.p;
        for (unsigned i = 0; i < before.count; i++) {
            bus.mem[before.addr[i]] = (uint8_t)before.value[i];
        }

        int used = cpu_step(&cpu);
        total++;

        int ok = cpu.pc == after.pc && cpu.sp == after.s && cpu.a == after.a
              && cpu.x == after.x && cpu.y == after.y && cpu.p == after.p
              && (unsigned)used == cycles;
        for (unsigned i = 0; i < after.count; i++) {
            if (bus.mem[after.addr[i]] != after.value[i]) {
                ok = 0;
            }
        }

        if (!ok) {
            failed++;
            if (failed <= 3) {
                printf("  FAIL %s: got PC=%04X S=%02X A=%02X X=%02X Y=%02X P=%02X"
                       " cycles=%d, expected %04X %02X %02X %02X %02X %02X %u\n",
                       name, cpu.pc, cpu.sp, cpu.a, cpu.x, cpu.y, cpu.p, used,
                       after.pc, after.s, after.a, after.x, after.y, after.p,
                       cycles);
            }
        }
    }

    printf("%ld tests, %ld failed\n", total, failed);
    return failed == 0 ? 0 : 1;
}
