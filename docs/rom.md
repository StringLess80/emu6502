# The system: memory map, ROM and BIOS

## Memory map

| Address       | Device          | Notes                                   |
|---------------|-----------------|-----------------------------------------|
| $0000-$3FFF   | RAM, 16 KiB     | Zero page, stack, then free RAM         |
| $4000-$4FFF   | —               | Free, reads $FF                         |
| $5000-$5FFF   | ACIA 6551       | 4 registers, mirrored                   |
| $6000-$6FFF   | VIA 6522        | 16 registers, mirrored                  |
| $7000-$7FFF   | —               | Free, reads $FF                         |
| $8000-$FFFF   | ROM, 32 KiB     | Writes are ignored                      |

A15 and A14 select a 16 KiB block. Inside the $4000-$7FFF block, A13 and
A12 select one of four 4 KiB I/O slots. A device answers anywhere in its
slot: the ACIA uses A0-A1 and the VIA uses A0-A3, so `$5001` and `$5FF1`
reach the same ACIA register.

### I/O registers

| Address | Register            | Address | Register              |
|---------|---------------------|---------|-----------------------|
| $5000   | ACIA data           | $6000   | VIA port B            |
| $5001   | ACIA status         | $6001   | VIA port A            |
| $5002   | ACIA command        | $6002   | VIA DDRB              |
| $5003   | ACIA control        | $6003   | VIA DDRA              |
|         |                     | $6004-$6009 | VIA timers 1 and 2 |
|         |                     | $600B   | VIA ACR               |
|         |                     | $600D   | VIA IFR               |
|         |                     | $600E   | VIA IER               |

ACIA status bit 3 (`$08`) is set when a key is waiting. Bit 4 (`$10`) is
set when the ACIA is ready to send.

## ROM layout

| Address       | Contents                                                  |
|---------------|-----------------------------------------------------------|
| $8000         | `JMP COLD_START`: type `8000R` in WozMon to start BASIC  |
| $8003-$9EE4   | Microsoft BASIC (OSI version 1.0 rev 3.2, 6-digit floats) |
| $9EE5-$FCFF   | Free: about 24 KiB for your own ROM code                  |
| $FD00         | `JMP CHRIN`                                               |
| $FD03         | `JMP CHROUT`                                              |
| $FE00         | WozMon (the reset routine in the BIOS jumps here)         |
| $FFEB-$FFF9   | The five entry points OSI BASIC expects from its monitor  |
| $FFFA-$FFFF   | NMI, RESET, IRQ vectors                                   |

## BIOS

### Entry points for your own programs

| Address | Routine | Does                                                   |
|---------|---------|--------------------------------------------------------|
| $FD00   | CHRIN   | If a key is waiting: read it, echo it, return it in A with C=1. Otherwise C=0 |
| $FD03   | CHROUT  | Send A to the terminal. A, X and Y are preserved       |
| $FE00   | WozMon  | Jump here to return to the monitor                     |

User programs can live in RAM from $0300. BASIC stores its programs from
$0300 upwards, so running BASIC overwrites them.

A minimal program that prints `HELLO!` and returns to WozMon. Type it at
the WozMon prompt, then run it with `300R`:

```
0300: A2 00 BD 10 03 F0 06 20 03 FD E8 D0 F5 4C 00 FE
0310: 48 45 4C 4C 4F 21 0D 00
```

### What the BIOS does for BASIC

- **A key reader that doesn't echo.** BASIC echoes what you type by itself.
- **A Ctrl+C check.** Ctrl+C stops a running program with `BREAK IN`.
- **A 64-key typeahead buffer** in page 2 ($0280-$02BF), so keys typed
  while a program runs aren't lost.
- **An output routine that clears bit 7.** BASIC marks the last letter of
  its error messages that way, and a terminal would show garbage.

LOAD and SAVE do nothing.

### BASIC notes

- At start-up BASIC asks MEMORY SIZE? and TERMINAL WIDTH?. Press Enter
  for both, and BASIC finds the RAM by itself (15615 bytes free).
- This is the 1977 dialect: there is no `CLS`, `ELSE` or `DELETE`. To
  clear the screen, print the ANSI escape codes:
  `PRINT CHR$(27);"[2J";CHR$(27);"[H"`. To delete a line, type its
  number alone and press Enter.
- Numbers have about 6 significant digits.

## Building the ROM

You need ca65/ld65 from [cc65](https://cc65.github.io/), and the Microsoft
BASIC sources reconstructed in
[mist64/msbasic](https://github.com/mist64/msbasic):

```
git clone https://github.com/mist64/msbasic
cd rom
MSBASIC=../msbasic ./build.sh
```

This produces `rom.bin`, exactly 32768 bytes. Offset 0 of the file is
$8000.
