/*
 * sbc.h - The emulated 6502 single-board computer.
 *
 * Memory map:
 *
 *   $0000-$3FFF  RAM   16 KiB
 *   $4000-$4FFF  -     free (nothing answers, reads $FF)
 *   $5000-$5FFF  ACIA  6551, 4 registers, mirrored
 *   $6000-$6FFF  VIA   6522, 16 registers, mirrored
 *   $7000-$7FFF  -     free (nothing answers, reads $FF)
 *   $8000-$FFFF  ROM   32 KiB
 */
#ifndef SBC_H
#define SBC_H

#include <stdint.h>

#include "acia.h"
#include "bus.h"
#include "via.h"

#define RAM_SIZE   0x4000   /* 16 KiB */
#define ROM_SIZE   0x8000   /* 32 KiB */
#define ROM_START  0x8000

struct Bus {
    uint8_t ram[RAM_SIZE];
    uint8_t rom[ROM_SIZE];
    Via     via;
    Acia    acia;
};

/* Fill RAM with zeros, erase the ROM, reset the chips. */
void sbc_init(Bus *sbc);

/* Press the reset button: reset the chips, keep RAM and ROM. */
void sbc_reset_chips(Bus *sbc);

/* Write a byte like an EPROM programmer would: ROM is writable too. */
void sbc_load_byte(Bus *sbc, uint16_t addr, uint8_t value);

#endif /* SBC_H */
