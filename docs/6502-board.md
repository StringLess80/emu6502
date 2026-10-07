# The 6502 board: hardware and ROM

## Memory map

| Address       | Chip            | Selected by                     |
|---------------|-----------------|---------------------------------|
| $0000-$3FFF   | RAM (16 KiB)    | U4A /1Y0 (A15=0, A14=0)         |
| $4000-$4FFF   | free            | U4B /2Y0 (to expansion header)  |
| $5000-$5FFF   | ACIA 6551       | U4B /2Y1                        |
| $6000-$6FFF   | VIA 6522        | U4B /2Y2                        |
| $7000-$7FFF   | free            | U4B /2Y3 (to expansion header)  |
| $8000-$FFFF   | ROM (32 KiB)    | A15=1                           |

The whole 32 KiB of the AT28C256 is visible. MS BASIC uses less than a
third of it and reports 15615 bytes free.

## Address decoding

The decoding uses a 74HC139 (dual 2-to-4 decoder) and one gate of a
74HC00.

**U4A (first half of the 74HC139): selects the 16 KiB block**

| Pin            | Connect to                                |
|----------------|-------------------------------------------|
| /1E (1)        | GND                                       |
| 1A0 (2)        | A14                                       |
| 1A1 (3)        | A15                                       |
| /1Y0 (4)       | RAMCS (to the RAM's φ2 gating)            |
| /1Y1 (5)       | /2E of U4B (pin 15)                       |
| /1Y2, /1Y3     | not connected                             |

**U4B (second half): splits $4000-$7FFF into four 4 KiB I/O slots**

| Pin            | Connect to                                |
|----------------|-------------------------------------------|
| /2E (15)       | /1Y1 (pin 5)                              |
| 2A0 (14)       | A12                                       |
| 2A1 (13)       | A13                                       |
| /2Y0 (12)      | expansion header ($4000)                  |
| /2Y1 (11)      | ACIA /CS1 ($5000)                         |
| /2Y2 (10)      | VIA /CS2 ($6000)                          |
| /2Y3 (9)       | expansion header ($7000)                  |

**ROM chip enable:** a 74HC00 gate used as an inverter. Both inputs go
to A15, and the output goes to the EEPROM's /CE, so the EEPROM is
selected whenever A15 = 1. Connect A0-A14 of the EEPROM to A0-A14 of the
CPU. /WE goes to +5V.

**RAM:** A0-A13 of the CPU go to the HM62256, and the RAM's A14 is tied
to GND (16 KiB). Its chip select is RAMCS gated with φ2 (two NAND gates),
so the RAM is only enabled while φ2 is high.

**I/O:** the VIA's RS0-RS3 = A0-A3, CS1 = +5V. The ACIA's RS0-RS1 =
A0-A1, CS0 = +5V, with a 1.8432 MHz crystal for the baud rate generator.

The I/O selects go through two decoder stages, about 40 ns in total with
74HC parts. That is nothing at 1 MHz, and fine up to several MHz.

## CPU notes

- **/IRQ (pin 4) and /NMI (pin 6):** pull each up to +5V with about 3.3k.
- **RDY (pin 2):** pull up with 1k. **SO (pin 38):** tie high.
- **NMOS 6502** (MOS / Rockwell R6502): the clock goes into φ0
  (pin 37). Use φ2 out (pin 39) for the RAM gating and for the 6522/6551.
- **WDC W65C02S:** the clock goes into PHI2 (pin 37). Use that same clock
  net for the gating, since WDC advises against using PHI2O (pin 39). Tie
  BE (pin 36) high, or the buses float. Pin 1 is VPB, an output: leave it
  unconnected (it is GND on the NMOS part).

Optional: a 10k resistor network pulling D0-D7 up to +5V makes empty
addresses read $FF, as they do in the emulator. BASIC's "MEMORY SIZE?"
probe then stops cleanly at $4000. If it ever doesn't, answer the
question with 16384.

## The ROM

The `rom/` folder builds a 32 KiB image:

| Address       | Contents                                                  |
|---------------|-----------------------------------------------------------|
| $8000         | `JMP COLD_START`: type `8000R` in WozMon to start BASIC  |
| $8003-$9EE4   | MS BASIC (OSI version 1.0 rev 3.2, 6-digit floats)        |
| $9EE5-$FCFF   | free: about 24 KiB for your own ROM code                  |
| $FD00         | `JMP CHRIN`                                               |
| $FD03         | `JMP CHROUT`                                              |
| $FE00         | WozMon (the reset routine in the BIOS jumps here)         |
| $FFEB-$FFF9   | the five entry points OSI BASIC expects from its monitor  |
| $FFFA-$FFFF   | NMI, RESET, IRQ vectors                                   |

The BIOS gives BASIC:

- **A key reader that doesn't echo.** BASIC echoes what you type by itself.
- **A Ctrl+C check.** Ctrl+C stops a running program with `BREAK IN`.
- **A 64-key typeahead buffer** in page 2 ($0280-$02BF), so keys typed
  while a program runs aren't lost.
- **An output routine that clears bit 7.** BASIC marks the last letter of
  its error messages that way, and a serial terminal would show garbage.

LOAD and SAVE do nothing.

### BIOS entry points for your own programs

| Address | Routine | Does                                                   |
|---------|---------|--------------------------------------------------------|
| $FD00   | CHRIN   | If a key is waiting: read it, echo it, return it in A with C=1. Otherwise C=0 |
| $FD03   | CHROUT  | Send A to the serial port. A, X, Y preserved           |
| $FE00   | WozMon  | Jump here to return to the monitor                     |

User programs can live in RAM from $0300 (or $0500 if you also want to
keep page 3 free). BASIC stores its programs from $0300 upwards, so
running BASIC overwrites them.

### Building it

You need ca65/ld65 from cc65, and the Microsoft BASIC sources
reconstructed in github.com/mist64/msbasic:

```
git clone https://github.com/mist64/msbasic
cd rom
MSBASIC=../msbasic ./build.sh
```

This produces `rom.bin`, exactly 32768 bytes. Burn it into the AT28C256
as it is: offset 0 of the file is $8000.
