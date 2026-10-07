/*
 * emu_wasm.c - The emulator core, exported to JavaScript.
 *
 * This file replaces main.c, terminal.c and loader.c of the terminal
 * version: the browser does the screen, the keyboard, the clock and the
 * file loading. The CPU and the devices are the very same C files.
 *
 * Every exported function has a plain integer interface, so JavaScript
 * never needs to know how the C structures are laid out.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cpu.h"
#include "opcodes.h"
#include "sbc.h"

#define EXPORT(name) __attribute__((export_name(#name)))

/* ------------------------------------------------------------------ */
/* The C library functions the core uses (see shim/string.h)           */
/* ------------------------------------------------------------------ */

/* With -mbulk-memory these builtins become single WebAssembly
 * instructions (memory.fill / memory.copy), not calls to themselves. */
void *memset(void *dst, int value, size_t n)
{
    __builtin_memset(dst, value, n);
    return dst;
}

void *memcpy(void *dst, const void *src, size_t n)
{
    __builtin_memcpy(dst, src, n);
    return dst;
}

/* ------------------------------------------------------------------ */
/* State                                                               */
/* ------------------------------------------------------------------ */

static Bus sbc;
static Cpu cpu;

/* Keys typed in the browser, waiting for the ACIA. */
#define KEYQ_SIZE 4096
static uint8_t keyq[KEYQ_SIZE];
static uint32_t keyq_head, keyq_tail, keyq_count;

/* Bytes the 6502 sent to the ACIA, waiting for the browser. */
#define OUT_SIZE 65536
static uint8_t out_buf[OUT_SIZE];
static uint32_t out_len;

/* One flag per address: stop before executing the instruction there. */
static uint8_t breakpoints[65536];

enum { RUN_OK = 0, RUN_BREAKPOINT = 1, RUN_HALTED = 2 };

/* ------------------------------------------------------------------ */
/* Setup                                                               */
/* ------------------------------------------------------------------ */

/* Power on: clear RAM, erase ROM, reset everything. */
EXPORT(emu_init) void emu_init(void)
{
    sbc_init(&sbc);
    cpu_init(&cpu, &sbc);
    keyq_head = keyq_tail = keyq_count = 0;
    out_len = 0;
}

/* Where JavaScript writes the ROM image ($8000-$FFFF, 32 KiB). */
EXPORT(emu_rom_ptr) uint8_t *emu_rom_ptr(void)
{
    return sbc.rom;
}

EXPORT(emu_rom_size) uint32_t emu_rom_size(void)
{
    return ROM_SIZE;
}

/* The reset button: reset the CPU and the devices, keep RAM. */
EXPORT(emu_reset) void emu_reset(void)
{
    sbc_reset_chips(&sbc);
    cpu_reset(&cpu);
    keyq_head = keyq_tail = keyq_count = 0;
}

/* Write a byte anywhere, ROM included, without side effects on I/O
 * beyond a normal write. Used to load programs and edit memory. */
EXPORT(emu_load_byte) void emu_load_byte(uint32_t addr, uint32_t value)
{
    sbc_load_byte(&sbc, (uint16_t)addr, (uint8_t)value);
}

/* ------------------------------------------------------------------ */
/* Running                                                             */
/* ------------------------------------------------------------------ */

static void after_instruction(int used)
{
    via_tick(&sbc.via, used);

    uint8_t byte;
    if (acia_transmit(&sbc.acia, &byte) && out_len < OUT_SIZE) {
        out_buf[out_len++] = byte;
    }

    if (!sbc.acia.rx_full && keyq_count > 0) {
        acia_receive(&sbc.acia, keyq[keyq_head]);
        keyq_head = (keyq_head + 1) % KEYQ_SIZE;
        keyq_count--;
    }
}

/* Run for (at least) the given number of cycles. Stops early on a
 * breakpoint or an illegal opcode. A breakpoint on the very first
 * instruction is ignored when skip_first is set, so that "continue"
 * can move past the breakpoint it stopped on. */
EXPORT(emu_run) int emu_run(uint32_t cycles, int skip_first)
{
    uint64_t end = cpu.cycles + cycles;
    bool first = true;

    while (cpu.cycles < end) {
        if (cpu.halted) {
            return RUN_HALTED;
        }
        if (breakpoints[cpu.pc] && !(first && skip_first)) {
            return RUN_BREAKPOINT;
        }
        first = false;
        after_instruction(cpu_step(&cpu));
    }
    return cpu.halted ? RUN_HALTED : RUN_OK;
}

/* Execute exactly one instruction. Returns its cycles. */
EXPORT(emu_step) int emu_step(void)
{
    if (cpu.halted) {
        return 0;
    }
    int used = cpu_step(&cpu);
    after_instruction(used);
    return used;
}

/* ------------------------------------------------------------------ */
/* Terminal                                                            */
/* ------------------------------------------------------------------ */

EXPORT(emu_key) int emu_key(uint32_t byte)
{
    if (keyq_count == KEYQ_SIZE) {
        return 0;
    }
    keyq[keyq_tail] = (uint8_t)byte;
    keyq_tail = (keyq_tail + 1) % KEYQ_SIZE;
    keyq_count++;
    return 1;
}

EXPORT(emu_key_count) uint32_t emu_key_count(void)
{
    return keyq_count;
}

EXPORT(emu_out_ptr) uint8_t *emu_out_ptr(void)
{
    return out_buf;
}

EXPORT(emu_out_len) uint32_t emu_out_len(void)
{
    return out_len;
}

EXPORT(emu_out_clear) void emu_out_clear(void)
{
    out_len = 0;
}

/* ------------------------------------------------------------------ */
/* Inspection                                                          */
/* ------------------------------------------------------------------ */

enum { REG_A, REG_X, REG_Y, REG_SP, REG_P, REG_PC, REG_HALTED };

EXPORT(emu_reg) uint32_t emu_reg(uint32_t which)
{
    switch (which) {
    case REG_A:      return cpu.a;
    case REG_X:      return cpu.x;
    case REG_Y:      return cpu.y;
    case REG_SP:     return cpu.sp;
    case REG_P:      return cpu.p;
    case REG_PC:     return cpu.pc;
    case REG_HALTED: return cpu.halted;
    default:         return 0;
    }
}

/* Change a register from the debugger. */
EXPORT(emu_set_reg) void emu_set_reg(uint32_t which, uint32_t value)
{
    switch (which) {
    case REG_A:  cpu.a = (uint8_t)value;   break;
    case REG_X:  cpu.x = (uint8_t)value;   break;
    case REG_Y:  cpu.y = (uint8_t)value;   break;
    case REG_SP: cpu.sp = (uint8_t)value;  break;
    case REG_P:  cpu.p = (uint8_t)(value | FLAG_U); break;
    case REG_PC: cpu.pc = (uint16_t)value; cpu.halted = false; break;
    default: break;
    }
}

/* A 64-bit counter does not fit a 32-bit integer; a double holds it
 * exactly up to 2^53 cycles (285 years at 1 MHz). */
EXPORT(emu_cycles) double emu_cycles(void)
{
    return (double)cpu.cycles;
}

/* Read memory without side effects. */
EXPORT(emu_peek) uint32_t emu_peek(uint32_t addr)
{
    return bus_peek(&sbc, (uint16_t)addr);
}

/* Fill a buffer with a block of memory, without side effects. */
static uint8_t peek_buf[65536];

EXPORT(emu_peek_block) uint8_t *emu_peek_block(uint32_t addr, uint32_t len)
{
    if (len > sizeof peek_buf) {
        len = sizeof peek_buf;
    }
    for (uint32_t i = 0; i < len; i++) {
        peek_buf[i] = bus_peek(&sbc, (uint16_t)(addr + i));
    }
    return peek_buf;
}

EXPORT(emu_breakpoints) uint8_t *emu_breakpoints(void)
{
    return breakpoints;
}

/* The opcode table, for the disassembler written in JavaScript. */
EXPORT(emu_op_name) const char *emu_op_name(uint32_t opcode)
{
    return OPCODES[opcode & 0xFF].name;
}

EXPORT(emu_op_mode) uint32_t emu_op_mode(uint32_t opcode)
{
    return OPCODES[opcode & 0xFF].mode;
}

EXPORT(emu_op_len) uint32_t emu_op_len(uint32_t opcode)
{
    return (uint32_t)mode_length(OPCODES[opcode & 0xFF].mode);
}

/* Device state. */
enum {
    DEV_ORA, DEV_ORB, DEV_DDRA, DEV_DDRB, DEV_PORTA, DEV_PORTB,
    DEV_T1, DEV_T1LATCH, DEV_T2, DEV_ACR, DEV_PCR, DEV_IFR, DEV_IER,
    DEV_ACIA_STATUS, DEV_ACIA_COMMAND, DEV_ACIA_CONTROL, DEV_ACIA_RXDATA
};

EXPORT(emu_dev) uint32_t emu_dev(uint32_t which)
{
    const Via *v = &sbc.via;
    switch (which) {
    case DEV_ORA:          return v->ora;
    case DEV_ORB:          return v->orb;
    case DEV_DDRA:         return v->ddra;
    case DEV_DDRB:         return v->ddrb;
    case DEV_PORTA:        return via_port_a(v);
    case DEV_PORTB:        return via_port_b(v);
    case DEV_T1:           return v->t1_counter;
    case DEV_T1LATCH:      return v->t1_latch;
    case DEV_T2:           return v->t2_counter;
    case DEV_ACR:          return v->acr;
    case DEV_PCR:          return v->pcr;
    case DEV_IFR:          return via_peek(v, VIA_IFR);
    case DEV_IER:          return via_peek(v, VIA_IER);
    case DEV_ACIA_STATUS:  return acia_peek(&sbc.acia, 1);
    case DEV_ACIA_COMMAND: return sbc.acia.command;
    case DEV_ACIA_CONTROL: return sbc.acia.control;
    case DEV_ACIA_RXDATA:  return sbc.acia.rx_data;
    default:               return 0;
    }
}
