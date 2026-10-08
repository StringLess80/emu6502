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

/* --- Undocumented opcodes --- */

static void test_lax_sax(void)
{
    const uint8_t prog[] = {
        0xA7, 0x10,         /* LAX $10   : A = X = mem[$10]     */
        0xA9, 0x0F,         /* LDA #$0F                         */
        0x87, 0x11,         /* SAX $11   : mem[$11] = A AND X   */
    };
    setup(prog, sizeof prog);
    bus.mem[0x10] = 0x9C;
    CHECK(cpu_step(&cpu) == 3);
    CHECK(cpu.a == 0x9C && cpu.x == 0x9C);
    CHECK(cpu.p & FLAG_N);
    run(2);
    CHECK(bus.mem[0x11] == 0x0C);
}

static void test_dcp_isc(void)
{
    const uint8_t prog[] = {
        0xA9, 0x41,         /* LDA #$41                          */
        0xC7, 0x10,         /* DCP $10 : mem-- then CMP          */
        0x38,               /* SEC                               */
        0xE7, 0x11,         /* ISC $11 : mem++ then SBC          */
    };
    setup(prog, sizeof prog);
    bus.mem[0x10] = 0x42;
    bus.mem[0x11] = 0x0F;
    run(2);
    CHECK(bus.mem[0x10] == 0x41);
    CHECK((cpu.p & FLAG_Z) && (cpu.p & FLAG_C));   /* A == mem */
    run(2);
    CHECK(bus.mem[0x11] == 0x10);
    CHECK(cpu.a == 0x31);
}

static void test_slo_and_alr(void)
{
    const uint8_t prog[] = {
        0xA9, 0x01,         /* LDA #$01                           */
        0x07, 0x10,         /* SLO $10 : mem <<= 1, then A |= mem */
        0x4B, 0x03,         /* ALR #$03: A &= 3, then LSR A       */
    };
    setup(prog, sizeof prog);
    bus.mem[0x10] = 0xC0;
    run(2);
    CHECK(bus.mem[0x10] == 0x80);
    CHECK(cpu.a == 0x81);
    CHECK(cpu.p & FLAG_C);           /* bit 7 shifted out of $C0 */
    run(1);
    CHECK(cpu.a == 0x00);
    CHECK((cpu.p & FLAG_C) && (cpu.p & FLAG_Z));
}

static void test_nop_variants(void)
{
    const uint8_t prog[] = {
        0x80, 0xFF,         /* NOP #$FF  : 2 bytes, 2 cycles      */
        0xA2, 0x01,         /* LDX #$01                           */
        0x1C, 0xFF, 0x02,   /* NOP $02FF,X : crosses a page       */
    };
    setup(prog, sizeof prog);
    CHECK(cpu_step(&cpu) == 2);
    CHECK(cpu.pc == 0x0202);
    run(1);
    CHECK(cpu_step(&cpu) == 5);
    CHECK(cpu.pc == 0x0207);
}

static void test_jam_locks_up(void)
{
    const uint8_t prog[] = { 0x02 };              /* JAM */
    setup(prog, sizeof prog);
    run(1);
    CHECK(cpu.halted);
    CHECK(cpu.pc == 0x0200);
    CHECK(cpu_step(&cpu) == 0);                   /* stays jammed */
    cpu_reset(&cpu);
    CHECK(!cpu.halted);                           /* reset revives it */
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
    test_lax_sax();
    test_dcp_isc();
    test_slo_and_alr();
    test_nop_variants();
    test_jam_locks_up();

    printf("%d checks, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? 0 : 1;
}
