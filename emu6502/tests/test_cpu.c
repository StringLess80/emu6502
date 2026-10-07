/*
 * test_cpu.c - Small, readable tests for single instructions.
 *
 * Each test puts a few bytes of machine code at $0200, runs a fixed
 * number of instructions and checks registers, flags and memory.
 */
#include <stdio.h>
#include <string.h>

#include "../src/cpu.h"
#include "flatbus.h"

static int tests_run = 0;
static int tests_failed = 0;

/* CHECK(condition): if the condition is false, print where and what.
 * __FILE__ and __LINE__ are filled in by the preprocessor, and
 * #cond turns the condition itself into a string. */
#define CHECK(cond)                                                    \
    do {                                                               \
        tests_run++;                                                   \
        if (!(cond)) {                                                 \
            tests_failed++;                                            \
            printf("%s:%d: FAILED: %s\n", __FILE__, __LINE__, #cond);  \
        }                                                              \
    } while (0)

static Bus bus;     /* 64 KiB, static so it is not on the stack */
static Cpu cpu;

/* Put the program at $0200, point the reset vector at it, reset. */
static void setup(const uint8_t *program, size_t length)
{
    memset(bus.mem, 0, sizeof bus.mem);
    memcpy(&bus.mem[0x0200], program, length);
    bus.mem[0xFFFC] = 0x00;
    bus.mem[0xFFFD] = 0x02;
    cpu_init(&cpu, &bus);
    cpu_reset(&cpu);
}

static void run(int instructions)
{
    for (int i = 0; i < instructions; i++) {
        cpu_step(&cpu);
    }
}

static void test_lda_flags(void)
{
    const uint8_t prog[] = { 0xA9, 0x00,     /* LDA #$00 */
                             0xA9, 0x80 };   /* LDA #$80 */
    setup(prog, sizeof prog);

    run(1);
    CHECK(cpu.a == 0x00);
    CHECK(cpu.p & FLAG_Z);
    CHECK(!(cpu.p & FLAG_N));

    run(1);
    CHECK(cpu.a == 0x80);
    CHECK(!(cpu.p & FLAG_Z));
    CHECK(cpu.p & FLAG_N);
}

static void test_sta_zero_page(void)
{
    const uint8_t prog[] = { 0xA9, 0x42,     /* LDA #$42 */
                             0x85, 0x10 };   /* STA $10  */
    setup(prog, sizeof prog);
    run(2);
    CHECK(bus.mem[0x10] == 0x42);
}

static void test_adc_carry_and_overflow(void)
{
    const uint8_t prog[] = { 0x18,           /* CLC                     */
                             0xA9, 0x7F,     /* LDA #$7F   (+127)       */
                             0x69, 0x01,     /* ADC #$01   -> $80, V=1  */
                             0xA9, 0xFF,     /* LDA #$FF                */
                             0x69, 0x01 };   /* ADC #$01   -> $00, C=1  */
    setup(prog, sizeof prog);

    run(3);
    CHECK(cpu.a == 0x80);
    CHECK(cpu.p & FLAG_V);
    CHECK(!(cpu.p & FLAG_C));

    run(2);
    CHECK(cpu.a == 0x00);
    CHECK(cpu.p & FLAG_C);
    CHECK(cpu.p & FLAG_Z);
    CHECK(!(cpu.p & FLAG_V));
}

static void test_adc_decimal(void)
{
    const uint8_t prog[] = { 0xF8,           /* SED           */
                             0x18,           /* CLC           */
                             0xA9, 0x19,     /* LDA #$19      */
                             0x69, 0x28 };   /* ADC #$28 = 47 */
    setup(prog, sizeof prog);
    run(4);
    CHECK(cpu.a == 0x47);
    CHECK(!(cpu.p & FLAG_C));
}

static void test_sbc_borrow(void)
{
    const uint8_t prog[] = { 0x38,           /* SEC (no borrow) */
                             0xA9, 0x05,     /* LDA #5          */
                             0xE9, 0x07 };   /* SBC #7 -> -2    */
    setup(prog, sizeof prog);
    run(3);
    CHECK(cpu.a == 0xFE);
    CHECK(!(cpu.p & FLAG_C));      /* a borrow happened */
    CHECK(cpu.p & FLAG_N);
}

static void test_jsr_rts(void)
{
    const uint8_t prog[] = { 0x20, 0x10, 0x02,   /* $0200 JSR $0210 */
                             0xEA };             /* $0203 NOP       */
    setup(prog, sizeof prog);
    bus.mem[0x0210] = 0x60;                      /* $0210 RTS       */

    run(1);
    CHECK(cpu.pc == 0x0210);
    CHECK(cpu.sp == 0xFB);
    CHECK(bus.mem[0x01FD] == 0x02);   /* pushed $0202, high byte first */
    CHECK(bus.mem[0x01FC] == 0x02);

    run(1);
    CHECK(cpu.pc == 0x0203);
    CHECK(cpu.sp == 0xFD);
}

static void test_branch_cycles(void)
{
    const uint8_t prog[] = { 0xA2, 0x03,         /* LDX #3          */
                             0xCA,               /* loop: DEX       */
                             0xD0, 0xFD };       /* BNE loop        */
    setup(prog, sizeof prog);

    run(1);
    run(1);
    CHECK(cpu_step(&cpu) == 3);       /* taken, same page: 2 + 1 */
    run(3);                           /* DEX, BNE (taken), DEX   */
    CHECK(cpu.x == 0);
    CHECK(cpu_step(&cpu) == 2);       /* not taken: 2 */
}

static void test_jmp_indirect_bug(void)
{
    const uint8_t prog[] = { 0x6C, 0xFF, 0x03 };  /* JMP ($03FF) */
    setup(prog, sizeof prog);
    bus.mem[0x03FF] = 0x34;
    bus.mem[0x0400] = 0x12;   /* a correct CPU would use this ... */
    bus.mem[0x0300] = 0x56;   /* ... but the 6502 uses this one   */
    run(1);
    CHECK(cpu.pc == 0x5634);
}

static void test_illegal_opcode_halts(void)
{
    const uint8_t prog[] = { 0x02 };              /* not an instruction */
    setup(prog, sizeof prog);
    run(1);
    CHECK(cpu.halted);
    CHECK(cpu.pc == 0x0200);
}

int main(void)
{
    test_lda_flags();
    test_sta_zero_page();
    test_adc_carry_and_overflow();
    test_adc_decimal();
    test_sbc_borrow();
    test_jsr_rts();
    test_branch_cycles();
    test_jmp_indirect_bug();
    test_illegal_opcode_halts();

    printf("%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
