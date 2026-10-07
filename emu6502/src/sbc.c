/*
 * sbc.c - Address decoding: which chip answers at which address.
 */
#include <string.h>

#include "sbc.h"

enum { CHIP_RAM, CHIP_VIA, CHIP_ACIA, CHIP_ROM, CHIP_NONE };

/* A15 A14 select a 16 KiB block; inside the $4000 block, A13 A12
 * select a 4 KiB I/O slot. */
static int select_chip(uint16_t addr)
{
    switch (addr >> 14) {                 /* A15 A14 */
    case 0:
        return CHIP_RAM;                  /* $0000-$3FFF */
    case 1:
        switch ((addr >> 12) & 0x03) {    /* A13 A12 */
        case 1:  return CHIP_ACIA;        /* $5000-$5FFF */
        case 2:  return CHIP_VIA;         /* $6000-$6FFF */
        default: return CHIP_NONE;        /* $4000, $7000: free */
        }
    default:
        return CHIP_ROM;                  /* $8000-$FFFF */
    }
}

void sbc_init(Bus *sbc)
{
    memset(sbc->ram, 0, sizeof sbc->ram);
    memset(sbc->rom, 0xFF, sizeof sbc->rom);   /* empty ROM reads $FF */
    sbc_reset_chips(sbc);
}

void sbc_reset_chips(Bus *sbc)
{
    via_reset(&sbc->via);
    acia_reset(&sbc->acia);
}

uint8_t bus_read(Bus *sbc, uint16_t addr)
{
    switch (select_chip(addr)) {
    case CHIP_RAM:  return sbc->ram[addr & (RAM_SIZE - 1)];
    case CHIP_VIA:  return via_read(&sbc->via, (uint8_t)(addr & 0x0F));
    case CHIP_ACIA: return acia_read(&sbc->acia, (uint8_t)(addr & 0x03));
    case CHIP_ROM:  return sbc->rom[addr & (ROM_SIZE - 1)];
    default:        return 0xFF;   /* nothing answers: reads $FF */
    }
}

uint8_t bus_peek(Bus *sbc, uint16_t addr)
{
    switch (select_chip(addr)) {
    case CHIP_RAM:  return sbc->ram[addr & (RAM_SIZE - 1)];
    case CHIP_VIA:  return via_peek(&sbc->via, (uint8_t)(addr & 0x0F));
    case CHIP_ACIA: return acia_peek(&sbc->acia, (uint8_t)(addr & 0x03));
    case CHIP_ROM:  return sbc->rom[addr & (ROM_SIZE - 1)];
    default:        return 0xFF;
    }
}

void bus_write(Bus *sbc, uint16_t addr, uint8_t value)
{
    switch (select_chip(addr)) {
    case CHIP_RAM:
        sbc->ram[addr & (RAM_SIZE - 1)] = value;
        break;
    case CHIP_VIA:
        via_write(&sbc->via, (uint8_t)(addr & 0x0F), value);
        break;
    case CHIP_ACIA:
        acia_write(&sbc->acia, (uint8_t)(addr & 0x03), value);
        break;
    default:
        /* ROM is read-only, and nothing answers in the free slots. */
        break;
    }
}

void sbc_load_byte(Bus *sbc, uint16_t addr, uint8_t value)
{
    if (select_chip(addr) == CHIP_ROM) {
        sbc->rom[addr & (ROM_SIZE - 1)] = value;
    } else {
        bus_write(sbc, addr, value);
    }
}
