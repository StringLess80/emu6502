/*
 * cpu.c - A 6502 processor core, written to be read.
 *
 * The structure follows the real chip:
 *   1. small helpers to talk to the bus and to the stack
 *   2. one function per addressing mode (they compute an address)
 *   3. one function per kind of operation (they do the work)
 *   4. cpu_step(): fetch an opcode, then a big switch that combines
 *      an addressing mode with an operation.
 */
#include "cpu.h"
#include "opcodes.h"

/* ------------------------------------------------------------------ */
/* 1. Bus, stack and flag helpers                                      */
/* ------------------------------------------------------------------ */

static uint8_t read8(Cpu *cpu, uint16_t addr)
{
    return bus_read(cpu->bus, addr);
}

static void write8(Cpu *cpu, uint16_t addr, uint8_t value)
{
    bus_write(cpu->bus, addr, value);
}

/* The 6502 is little-endian: low byte first, high byte second. */
static uint16_t read16(Cpu *cpu, uint16_t addr)
{
    uint16_t lo = read8(cpu, addr);
    uint16_t hi = read8(cpu, (uint16_t)(addr + 1));
    return (uint16_t)(lo | (hi << 8));
}

/* Read the byte at PC and move PC forward. */
static uint8_t fetch8(Cpu *cpu)
{
    uint8_t value = read8(cpu, cpu->pc);
    cpu->pc++;
    return value;
}

static uint16_t fetch16(Cpu *cpu)
{
    uint16_t lo = fetch8(cpu);
    uint16_t hi = fetch8(cpu);
    return (uint16_t)(lo | (hi << 8));
}

/* The stack lives in page 1 ($0100-$01FF) and grows downwards. */
static void push8(Cpu *cpu, uint8_t value)
{
    write8(cpu, (uint16_t)(0x0100 + cpu->sp), value);
    cpu->sp--;
}

static uint8_t pull8(Cpu *cpu)
{
    cpu->sp++;
    return read8(cpu, (uint16_t)(0x0100 + cpu->sp));
}

static void push16(Cpu *cpu, uint16_t value)
{
    push8(cpu, (uint8_t)(value >> 8));     /* high byte first ... */
    push8(cpu, (uint8_t)(value & 0xFF));   /* ... so low is on top */
}

static uint16_t pull16(Cpu *cpu)
{
    uint16_t lo = pull8(cpu);
    uint16_t hi = pull8(cpu);
    return (uint16_t)(lo | (hi << 8));
}

/* Turn one flag on or off. */
static void set_flag(Cpu *cpu, uint8_t flag, bool on)
{
    if (on) {
        cpu->p |= flag;
    } else {
        cpu->p &= (uint8_t)~flag;
    }
}

static bool get_flag(const Cpu *cpu, uint8_t flag)
{
    return (cpu->p & flag) != 0;
}

/* Almost every instruction updates Z and N from its result. */
static void set_zn(Cpu *cpu, uint8_t value)
{
    set_flag(cpu, FLAG_Z, value == 0);
    set_flag(cpu, FLAG_N, (value & 0x80) != 0);
}

/* True when two addresses are in different 256-byte pages. */
static bool page_crossed(uint16_t a, uint16_t b)
{
    return (a & 0xFF00) != (b & 0xFF00);
}

/* ------------------------------------------------------------------ */
/* 2. Addressing modes: each returns the address of the operand        */
/* ------------------------------------------------------------------ */

/* For read instructions, crossing a page with an index costs one extra
 * cycle. Stores and read-modify-write instructions always pay it (it is
 * already in their base count), so they pass extra_cycle = false. */

static uint16_t addr_imm(Cpu *cpu)
{
    return cpu->pc++;          /* the operand is the next byte itself */
}

static uint16_t addr_zp(Cpu *cpu)
{
    return fetch8(cpu);
}

static uint16_t addr_zpx(Cpu *cpu)
{
    return (uint8_t)(fetch8(cpu) + cpu->x);   /* wraps inside page 0 */
}

static uint16_t addr_zpy(Cpu *cpu)
{
    return (uint8_t)(fetch8(cpu) + cpu->y);
}

static uint16_t addr_abs(Cpu *cpu)
{
    return fetch16(cpu);
}

static uint16_t addr_absx(Cpu *cpu, bool extra_cycle)
{
    uint16_t base = fetch16(cpu);
    uint16_t addr = (uint16_t)(base + cpu->x);
    if (extra_cycle && page_crossed(base, addr)) {
        cpu->cycles++;
    }
    return addr;
}

static uint16_t addr_absy(Cpu *cpu, bool extra_cycle)
{
    uint16_t base = fetch16(cpu);
    uint16_t addr = (uint16_t)(base + cpu->y);
    if (extra_cycle && page_crossed(base, addr)) {
        cpu->cycles++;
    }
    return addr;
}

/* ($zp,X): add X to the zero-page address, then read a pointer there. */
static uint16_t addr_indx(Cpu *cpu)
{
    uint8_t zp = (uint8_t)(fetch8(cpu) + cpu->x);
    uint16_t lo = read8(cpu, zp);
    uint16_t hi = read8(cpu, (uint8_t)(zp + 1));   /* wraps in page 0 */
    return (uint16_t)(lo | (hi << 8));
}

/* ($zp),Y: read a pointer from zero page, then add Y to it. */
static uint16_t addr_indy(Cpu *cpu, bool extra_cycle)
{
    uint8_t zp = fetch8(cpu);
    uint16_t lo = read8(cpu, zp);
    uint16_t hi = read8(cpu, (uint8_t)(zp + 1));
    uint16_t base = (uint16_t)(lo | (hi << 8));
    uint16_t addr = (uint16_t)(base + cpu->y);
    if (extra_cycle && page_crossed(base, addr)) {
        cpu->cycles++;
    }
    return addr;
}

/* ------------------------------------------------------------------ */
/* 3. Operations                                                       */
/* ------------------------------------------------------------------ */

/* --- Loads and stores --- */

static void lda(Cpu *cpu, uint16_t addr)
{
    cpu->a = read8(cpu, addr);
    set_zn(cpu, cpu->a);
}

static void ldx(Cpu *cpu, uint16_t addr)
{
    cpu->x = read8(cpu, addr);
    set_zn(cpu, cpu->x);
}

static void ldy(Cpu *cpu, uint16_t addr)
{
    cpu->y = read8(cpu, addr);
    set_zn(cpu, cpu->y);
}

/* --- Logic --- */

static void and_op(Cpu *cpu, uint16_t addr)
{
    cpu->a &= read8(cpu, addr);
    set_zn(cpu, cpu->a);
}

static void ora(Cpu *cpu, uint16_t addr)
{
    cpu->a |= read8(cpu, addr);
    set_zn(cpu, cpu->a);
}

static void eor(Cpu *cpu, uint16_t addr)
{
    cpu->a ^= read8(cpu, addr);
    set_zn(cpu, cpu->a);
}

/* BIT: Z from A AND m, but N and V are copied straight from m. */
static void bit(Cpu *cpu, uint16_t addr)
{
    uint8_t m = read8(cpu, addr);
    set_flag(cpu, FLAG_Z, (cpu->a & m) == 0);
    set_flag(cpu, FLAG_N, (m & 0x80) != 0);
    set_flag(cpu, FLAG_V, (m & 0x40) != 0);
}

/* --- Arithmetic --- */

static void adc(Cpu *cpu, uint16_t addr)
{
    uint8_t  m     = read8(cpu, addr);
    unsigned carry = get_flag(cpu, FLAG_C) ? 1 : 0;
    unsigned sum   = cpu->a + m + carry;   /* binary sum, up to 0x1FF */

    if (!get_flag(cpu, FLAG_D)) {
        /* Overflow: both inputs had the same sign, the result does not. */
        bool overflow = (~(cpu->a ^ m) & (cpu->a ^ sum) & 0x80) != 0;
        set_flag(cpu, FLAG_V, overflow);
        set_flag(cpu, FLAG_C, sum > 0xFF);
        cpu->a = (uint8_t)sum;
        set_zn(cpu, cpu->a);
        return;
    }

    /* Decimal (BCD) mode, as done by the original NMOS chip. */
    unsigned lo = (cpu->a & 0x0F) + (m & 0x0F) + carry;
    if (lo > 0x09) {
        lo += 0x06;                         /* decimal adjust low digit */
    }
    unsigned hi = (unsigned)(cpu->a >> 4) + (unsigned)(m >> 4) + (lo > 0x0F ? 1 : 0);

    set_flag(cpu, FLAG_Z, (sum & 0xFF) == 0);          /* from binary sum */
    set_flag(cpu, FLAG_N, (hi & 0x08) != 0);           /* before adjust   */
    set_flag(cpu, FLAG_V, (~(cpu->a ^ m) & (cpu->a ^ (hi << 4)) & 0x80) != 0);

    if (hi > 0x09) {
        hi += 0x06;                         /* decimal adjust high digit */
    }
    set_flag(cpu, FLAG_C, hi > 0x0F);
    cpu->a = (uint8_t)(((hi << 4) | (lo & 0x0F)) & 0xFF);
}

static void sbc(Cpu *cpu, uint16_t addr)
{
    uint8_t m      = read8(cpu, addr);
    int     borrow = get_flag(cpu, FLAG_C) ? 0 : 1;
    int     diff   = cpu->a - m - borrow;     /* may be negative */

    /* On the NMOS 6502 all four flags come from the binary result,
     * even in decimal mode. */
    bool overflow = ((cpu->a ^ m) & (cpu->a ^ diff) & 0x80) != 0;
    set_flag(cpu, FLAG_V, overflow);
    set_flag(cpu, FLAG_C, diff >= 0);         /* carry = "no borrow" */
    set_zn(cpu, (uint8_t)diff);

    if (!get_flag(cpu, FLAG_D)) {
        cpu->a = (uint8_t)diff;
        return;
    }

    int lo = (cpu->a & 0x0F) - (m & 0x0F) - borrow;
    int hi = (cpu->a >> 4) - (m >> 4);
    if (lo < 0) {
        lo -= 0x06;
        hi -= 1;
    }
    if (hi < 0) {
        hi -= 0x06;
    }
    /* hi and lo may be negative here. Shifting a negative int is
     * undefined behaviour in C, so we convert to unsigned first:
     * that conversion is well defined (it wraps around). */
    cpu->a = (uint8_t)((((unsigned)hi << 4) | ((unsigned)lo & 0x0F)) & 0xFF);
}

/* CMP, CPX and CPY all work like a subtraction that throws away
 * the result and keeps only the flags. */
static void compare(Cpu *cpu, uint8_t reg, uint16_t addr)
{
    uint8_t m = read8(cpu, addr);
    set_flag(cpu, FLAG_C, reg >= m);
    set_zn(cpu, (uint8_t)(reg - m));
}

/* --- Shifts and rotates (they work on a value and return it) --- */

static uint8_t do_asl(Cpu *cpu, uint8_t v)
{
    set_flag(cpu, FLAG_C, (v & 0x80) != 0);
    v = (uint8_t)(v << 1);
    set_zn(cpu, v);
    return v;
}

static uint8_t do_lsr(Cpu *cpu, uint8_t v)
{
    set_flag(cpu, FLAG_C, (v & 0x01) != 0);
    v = (uint8_t)(v >> 1);
    set_zn(cpu, v);
    return v;
}

static uint8_t do_rol(Cpu *cpu, uint8_t v)
{
    uint8_t carry_in = get_flag(cpu, FLAG_C) ? 0x01 : 0x00;
    set_flag(cpu, FLAG_C, (v & 0x80) != 0);
    v = (uint8_t)((v << 1) | carry_in);
    set_zn(cpu, v);
    return v;
}

static uint8_t do_ror(Cpu *cpu, uint8_t v)
{
    uint8_t carry_in = get_flag(cpu, FLAG_C) ? 0x80 : 0x00;
    set_flag(cpu, FLAG_C, (v & 0x01) != 0);
    v = (uint8_t)((v >> 1) | carry_in);
    set_zn(cpu, v);
    return v;
}

static uint8_t do_inc(Cpu *cpu, uint8_t v)
{
    v++;
    set_zn(cpu, v);
    return v;
}

static uint8_t do_dec(Cpu *cpu, uint8_t v)
{
    v--;
    set_zn(cpu, v);
    return v;
}

/* A "read-modify-write" instruction: read memory, change the value
 * with one of the do_xxx functions above, write it back. The parameter
 * op is a pointer to a function (see the chapter on function pointers). */
static void modify(Cpu *cpu, uint16_t addr, uint8_t (*op)(Cpu *, uint8_t))
{
    uint8_t value = read8(cpu, addr);
    write8(cpu, addr, op(cpu, value));
}

/* --- Branches --- */

static void branch(Cpu *cpu, bool condition)
{
    int8_t offset = (int8_t)fetch8(cpu);   /* signed: -128 .. +127 */
    if (condition) {
        uint16_t target = (uint16_t)(cpu->pc + offset);
        cpu->cycles += page_crossed(cpu->pc, target) ? 2 : 1;
        cpu->pc = target;
    }
}

/* ------------------------------------------------------------------ */
/* 4. Public functions                                                 */
/* ------------------------------------------------------------------ */

void cpu_init(Cpu *cpu, Bus *bus)
{
    cpu->a = 0;
    cpu->x = 0;
    cpu->y = 0;
    cpu->sp = 0;
    cpu->p = FLAG_U;
    cpu->pc = 0;
    cpu->cycles = 0;
    cpu->halted = false;
    cpu->bus = bus;
}

void cpu_reset(Cpu *cpu)
{
    cpu->sp = 0xFD;                    /* the chip ends up here        */
    cpu->p = FLAG_U | FLAG_I;          /* interrupts disabled          */
    cpu->pc = read16(cpu, 0xFFFC);     /* the reset vector             */
    cpu->cycles += 7;                  /* reset takes 7 cycles         */
    cpu->halted = false;
}

int cpu_step(Cpu *cpu)
{
    if (cpu->halted) {
        return 0;
    }

    uint64_t start  = cpu->cycles;
    uint16_t op_pc  = cpu->pc;
    uint8_t  opcode = fetch8(cpu);

    cpu->cycles += OPCODES[opcode].cycles;

    switch (opcode) {

    /* ---------- LDA / LDX / LDY ---------- */
    case 0xA9: lda(cpu, addr_imm(cpu));          break;
    case 0xA5: lda(cpu, addr_zp(cpu));           break;
    case 0xB5: lda(cpu, addr_zpx(cpu));          break;
    case 0xAD: lda(cpu, addr_abs(cpu));          break;
    case 0xBD: lda(cpu, addr_absx(cpu, true));   break;
    case 0xB9: lda(cpu, addr_absy(cpu, true));   break;
    case 0xA1: lda(cpu, addr_indx(cpu));         break;
    case 0xB1: lda(cpu, addr_indy(cpu, true));   break;

    case 0xA2: ldx(cpu, addr_imm(cpu));          break;
    case 0xA6: ldx(cpu, addr_zp(cpu));           break;
    case 0xB6: ldx(cpu, addr_zpy(cpu));          break;
    case 0xAE: ldx(cpu, addr_abs(cpu));          break;
    case 0xBE: ldx(cpu, addr_absy(cpu, true));   break;

    case 0xA0: ldy(cpu, addr_imm(cpu));          break;
    case 0xA4: ldy(cpu, addr_zp(cpu));           break;
    case 0xB4: ldy(cpu, addr_zpx(cpu));          break;
    case 0xAC: ldy(cpu, addr_abs(cpu));          break;
    case 0xBC: ldy(cpu, addr_absx(cpu, true));   break;

    /* ---------- STA / STX / STY ---------- */
    case 0x85: write8(cpu, addr_zp(cpu), cpu->a);           break;
    case 0x95: write8(cpu, addr_zpx(cpu), cpu->a);          break;
    case 0x8D: write8(cpu, addr_abs(cpu), cpu->a);          break;
    case 0x9D: write8(cpu, addr_absx(cpu, false), cpu->a);  break;
    case 0x99: write8(cpu, addr_absy(cpu, false), cpu->a);  break;
    case 0x81: write8(cpu, addr_indx(cpu), cpu->a);         break;
    case 0x91: write8(cpu, addr_indy(cpu, false), cpu->a);  break;

    case 0x86: write8(cpu, addr_zp(cpu), cpu->x);           break;
    case 0x96: write8(cpu, addr_zpy(cpu), cpu->x);          break;
    case 0x8E: write8(cpu, addr_abs(cpu), cpu->x);          break;

    case 0x84: write8(cpu, addr_zp(cpu), cpu->y);           break;
    case 0x94: write8(cpu, addr_zpx(cpu), cpu->y);          break;
    case 0x8C: write8(cpu, addr_abs(cpu), cpu->y);          break;

    /* ---------- Register transfers ---------- */
    case 0xAA: cpu->x = cpu->a;  set_zn(cpu, cpu->x); break;   /* TAX */
    case 0xA8: cpu->y = cpu->a;  set_zn(cpu, cpu->y); break;   /* TAY */
    case 0x8A: cpu->a = cpu->x;  set_zn(cpu, cpu->a); break;   /* TXA */
    case 0x98: cpu->a = cpu->y;  set_zn(cpu, cpu->a); break;   /* TYA */
    case 0xBA: cpu->x = cpu->sp; set_zn(cpu, cpu->x); break;   /* TSX */
    case 0x9A: cpu->sp = cpu->x;                      break;   /* TXS: no flags! */

    /* ---------- Stack ---------- */
    case 0x48: push8(cpu, cpu->a);                              break; /* PHA */
    case 0x68: cpu->a = pull8(cpu); set_zn(cpu, cpu->a);        break; /* PLA */
    case 0x08: push8(cpu, cpu->p | FLAG_B | FLAG_U);            break; /* PHP */
    case 0x28: cpu->p = (uint8_t)((pull8(cpu) & ~FLAG_B) | FLAG_U); break; /* PLP */

    /* ---------- Logic ---------- */
    case 0x29: and_op(cpu, addr_imm(cpu));        break;
    case 0x25: and_op(cpu, addr_zp(cpu));         break;
    case 0x35: and_op(cpu, addr_zpx(cpu));        break;
    case 0x2D: and_op(cpu, addr_abs(cpu));        break;
    case 0x3D: and_op(cpu, addr_absx(cpu, true)); break;
    case 0x39: and_op(cpu, addr_absy(cpu, true)); break;
    case 0x21: and_op(cpu, addr_indx(cpu));       break;
    case 0x31: and_op(cpu, addr_indy(cpu, true)); break;

    case 0x09: ora(cpu, addr_imm(cpu));           break;
    case 0x05: ora(cpu, addr_zp(cpu));            break;
    case 0x15: ora(cpu, addr_zpx(cpu));           break;
    case 0x0D: ora(cpu, addr_abs(cpu));           break;
    case 0x1D: ora(cpu, addr_absx(cpu, true));    break;
    case 0x19: ora(cpu, addr_absy(cpu, true));    break;
    case 0x01: ora(cpu, addr_indx(cpu));          break;
    case 0x11: ora(cpu, addr_indy(cpu, true));    break;

    case 0x49: eor(cpu, addr_imm(cpu));           break;
    case 0x45: eor(cpu, addr_zp(cpu));            break;
    case 0x55: eor(cpu, addr_zpx(cpu));           break;
    case 0x4D: eor(cpu, addr_abs(cpu));           break;
    case 0x5D: eor(cpu, addr_absx(cpu, true));    break;
    case 0x59: eor(cpu, addr_absy(cpu, true));    break;
    case 0x41: eor(cpu, addr_indx(cpu));          break;
    case 0x51: eor(cpu, addr_indy(cpu, true));    break;

    case 0x24: bit(cpu, addr_zp(cpu));            break;
    case 0x2C: bit(cpu, addr_abs(cpu));           break;

    /* ---------- Arithmetic ---------- */
    case 0x69: adc(cpu, addr_imm(cpu));           break;
    case 0x65: adc(cpu, addr_zp(cpu));            break;
    case 0x75: adc(cpu, addr_zpx(cpu));           break;
    case 0x6D: adc(cpu, addr_abs(cpu));           break;
    case 0x7D: adc(cpu, addr_absx(cpu, true));    break;
    case 0x79: adc(cpu, addr_absy(cpu, true));    break;
    case 0x61: adc(cpu, addr_indx(cpu));          break;
    case 0x71: adc(cpu, addr_indy(cpu, true));    break;

    case 0xE9: sbc(cpu, addr_imm(cpu));           break;
    case 0xE5: sbc(cpu, addr_zp(cpu));            break;
    case 0xF5: sbc(cpu, addr_zpx(cpu));           break;
    case 0xED: sbc(cpu, addr_abs(cpu));           break;
    case 0xFD: sbc(cpu, addr_absx(cpu, true));    break;
    case 0xF9: sbc(cpu, addr_absy(cpu, true));    break;
    case 0xE1: sbc(cpu, addr_indx(cpu));          break;
    case 0xF1: sbc(cpu, addr_indy(cpu, true));    break;

    /* ---------- Compares ---------- */
    case 0xC9: compare(cpu, cpu->a, addr_imm(cpu));         break;
    case 0xC5: compare(cpu, cpu->a, addr_zp(cpu));          break;
    case 0xD5: compare(cpu, cpu->a, addr_zpx(cpu));         break;
    case 0xCD: compare(cpu, cpu->a, addr_abs(cpu));         break;
    case 0xDD: compare(cpu, cpu->a, addr_absx(cpu, true));  break;
    case 0xD9: compare(cpu, cpu->a, addr_absy(cpu, true));  break;
    case 0xC1: compare(cpu, cpu->a, addr_indx(cpu));        break;
    case 0xD1: compare(cpu, cpu->a, addr_indy(cpu, true));  break;

    case 0xE0: compare(cpu, cpu->x, addr_imm(cpu));         break;
    case 0xE4: compare(cpu, cpu->x, addr_zp(cpu));          break;
    case 0xEC: compare(cpu, cpu->x, addr_abs(cpu));         break;

    case 0xC0: compare(cpu, cpu->y, addr_imm(cpu));         break;
    case 0xC4: compare(cpu, cpu->y, addr_zp(cpu));          break;
    case 0xCC: compare(cpu, cpu->y, addr_abs(cpu));         break;

    /* ---------- Increments and decrements ---------- */
    case 0xE6: modify(cpu, addr_zp(cpu), do_inc);           break;
    case 0xF6: modify(cpu, addr_zpx(cpu), do_inc);          break;
    case 0xEE: modify(cpu, addr_abs(cpu), do_inc);          break;
    case 0xFE: modify(cpu, addr_absx(cpu, false), do_inc);  break;

    case 0xC6: modify(cpu, addr_zp(cpu), do_dec);           break;
    case 0xD6: modify(cpu, addr_zpx(cpu), do_dec);          break;
    case 0xCE: modify(cpu, addr_abs(cpu), do_dec);          break;
    case 0xDE: modify(cpu, addr_absx(cpu, false), do_dec);  break;

    case 0xE8: cpu->x = do_inc(cpu, cpu->x); break;   /* INX */
    case 0xC8: cpu->y = do_inc(cpu, cpu->y); break;   /* INY */
    case 0xCA: cpu->x = do_dec(cpu, cpu->x); break;   /* DEX */
    case 0x88: cpu->y = do_dec(cpu, cpu->y); break;   /* DEY */

    /* ---------- Shifts and rotates ---------- */
    case 0x0A: cpu->a = do_asl(cpu, cpu->a);                break;
    case 0x06: modify(cpu, addr_zp(cpu), do_asl);           break;
    case 0x16: modify(cpu, addr_zpx(cpu), do_asl);          break;
    case 0x0E: modify(cpu, addr_abs(cpu), do_asl);          break;
    case 0x1E: modify(cpu, addr_absx(cpu, false), do_asl);  break;

    case 0x4A: cpu->a = do_lsr(cpu, cpu->a);                break;
    case 0x46: modify(cpu, addr_zp(cpu), do_lsr);           break;
    case 0x56: modify(cpu, addr_zpx(cpu), do_lsr);          break;
    case 0x4E: modify(cpu, addr_abs(cpu), do_lsr);          break;
    case 0x5E: modify(cpu, addr_absx(cpu, false), do_lsr);  break;

    case 0x2A: cpu->a = do_rol(cpu, cpu->a);                break;
    case 0x26: modify(cpu, addr_zp(cpu), do_rol);           break;
    case 0x36: modify(cpu, addr_zpx(cpu), do_rol);          break;
    case 0x2E: modify(cpu, addr_abs(cpu), do_rol);          break;
    case 0x3E: modify(cpu, addr_absx(cpu, false), do_rol);  break;

    case 0x6A: cpu->a = do_ror(cpu, cpu->a);                break;
    case 0x66: modify(cpu, addr_zp(cpu), do_ror);           break;
    case 0x76: modify(cpu, addr_zpx(cpu), do_ror);          break;
    case 0x6E: modify(cpu, addr_abs(cpu), do_ror);          break;
    case 0x7E: modify(cpu, addr_absx(cpu, false), do_ror);  break;

    /* ---------- Jumps and subroutines ---------- */
    case 0x4C:                                              /* JMP abs */
        cpu->pc = fetch16(cpu);
        break;

    case 0x6C: {                                            /* JMP (ind) */
        uint16_t ptr = fetch16(cpu);
        /* Famous NMOS bug: the high byte is read from the same page,
         * so JMP ($12FF) reads $12FF and $1200, not $1300. */
        uint16_t hi_addr = (uint16_t)((ptr & 0xFF00) | ((ptr + 1) & 0x00FF));
        uint16_t lo = read8(cpu, ptr);
        uint16_t hi = read8(cpu, hi_addr);
        cpu->pc = (uint16_t)(lo | (hi << 8));
        break;
    }

    case 0x20: {                                            /* JSR */
        uint16_t target = fetch16(cpu);
        push16(cpu, (uint16_t)(cpu->pc - 1));   /* address of last byte */
        cpu->pc = target;
        break;
    }

    case 0x60:                                              /* RTS */
        cpu->pc = (uint16_t)(pull16(cpu) + 1);
        break;

    case 0x00:                                              /* BRK */
        push16(cpu, (uint16_t)(cpu->pc + 1));   /* skips a padding byte */
        push8(cpu, cpu->p | FLAG_B | FLAG_U);
        set_flag(cpu, FLAG_I, true);
        cpu->pc = read16(cpu, 0xFFFE);
        break;

    case 0x40:                                              /* RTI */
        cpu->p = (uint8_t)((pull8(cpu) & ~FLAG_B) | FLAG_U);
        cpu->pc = pull16(cpu);
        break;

    /* ---------- Branches ---------- */
    case 0x10: branch(cpu, !get_flag(cpu, FLAG_N)); break;  /* BPL */
    case 0x30: branch(cpu,  get_flag(cpu, FLAG_N)); break;  /* BMI */
    case 0x50: branch(cpu, !get_flag(cpu, FLAG_V)); break;  /* BVC */
    case 0x70: branch(cpu,  get_flag(cpu, FLAG_V)); break;  /* BVS */
    case 0x90: branch(cpu, !get_flag(cpu, FLAG_C)); break;  /* BCC */
    case 0xB0: branch(cpu,  get_flag(cpu, FLAG_C)); break;  /* BCS */
    case 0xD0: branch(cpu, !get_flag(cpu, FLAG_Z)); break;  /* BNE */
    case 0xF0: branch(cpu,  get_flag(cpu, FLAG_Z)); break;  /* BEQ */

    /* ---------- Flag instructions ---------- */
    case 0x18: set_flag(cpu, FLAG_C, false); break;   /* CLC */
    case 0x38: set_flag(cpu, FLAG_C, true);  break;   /* SEC */
    case 0x58: set_flag(cpu, FLAG_I, false); break;   /* CLI */
    case 0x78: set_flag(cpu, FLAG_I, true);  break;   /* SEI */
    case 0xD8: set_flag(cpu, FLAG_D, false); break;   /* CLD */
    case 0xF8: set_flag(cpu, FLAG_D, true);  break;   /* SED */
    case 0xB8: set_flag(cpu, FLAG_V, false); break;   /* CLV */

    case 0xEA:                                        /* NOP */
        break;

    /* ---------- Anything else ---------- */
    default:
        /* An illegal opcode: stop, and leave PC pointing at it
         * so that whoever is debugging can see where it happened. */
        cpu->pc = op_pc;
        cpu->halted = true;
        break;
    }

    return (int)(cpu->cycles - start);
}
