========================================================================
 EMU6502 - THE SYSTEM: MEMORY MAP, ROM AND BIOS
========================================================================

 CONTENTS

    1.  Memory map
    2.  I/O registers
    3.  ROM layout
    4.  BIOS entry points
    5.  What the BIOS does for BASIC
    6.  BASIC notes
    7.  Building the ROM

========================================================================
 1. MEMORY MAP
========================================================================

    +---------------+-------------+-----------------------------------+
    | Address       | Device      | Notes                             |
    +---------------+-------------+-----------------------------------+
    | $0000-$3FFF   | RAM, 16K    | zero page, stack, then free RAM   |
    | $4000-$4FFF   | -           | free, reads $FF                   |
    | $5000-$5FFF   | ACIA 6551   | 4 registers, mirrored             |
    | $6000-$6FFF   | VIA 6522    | 16 registers, mirrored            |
    | $7000-$7FFF   | -           | free, reads $FF                   |
    | $8000-$FFFF   | ROM, 32K    | writes are ignored                |
    +---------------+-------------+-----------------------------------+

 A15 and A14 select a 16K block. Inside the $4000-$7FFF block, A13 and
 A12 select one of four 4K I/O slots. A device answers anywhere in its
 slot: the ACIA decodes A0-A1 and the VIA decodes A0-A3, so $5001 and
 $5FF1 reach the same ACIA register.

    A15 A14 A13 A12
     0   0   x   x    RAM          $0000-$3FFF
     0   1   0   0    free         $4000-$4FFF
     0   1   0   1    ACIA         $5000-$5FFF
     0   1   1   0    VIA          $6000-$6FFF
     0   1   1   1    free         $7000-$7FFF
     1   x   x   x    ROM          $8000-$FFFF

========================================================================
 2. I/O REGISTERS
========================================================================

 2.1 ACIA 6551 at $5000

    $5000 ............ data     read: received byte / write: send byte
    $5001 ............ status   bit 3 ($08) set: a key is waiting
                                bit 4 ($10) set: ready to send
                                (a write here resets the ACIA)
    $5002 ............ command
    $5003 ............ control

 2.2 VIA 6522 at $6000

    $6000 ............ ORB      port B
    $6001 ............ ORA      port A
    $6002 ............ DDRB     port B direction, 1 = output
    $6003 ............ DDRA     port A direction, 1 = output
    $6004 ............ T1CL     timer 1 counter low
    $6005 ............ T1CH     timer 1 counter high (starts T1)
    $6006 ............ T1LL     timer 1 latch low
    $6007 ............ T1LH     timer 1 latch high
    $6008 ............ T2CL     timer 2 counter low
    $6009 ............ T2CH     timer 2 counter high (starts T2)
    $600A ............ SR       shift register (stored only)
    $600B ............ ACR      auxiliary control
    $600C ............ PCR      peripheral control (stored only)
    $600D ............ IFR      interrupt flags: T1 = $40, T2 = $20
    $600E ............ IER      interrupt enable
    $600F ............ ORA      port A, no handshake

 From BASIC the VIA is at 24576: POKE 24578,255 makes port B an output,
 then POKE 24576,X sets its pins.

========================================================================
 3. ROM LAYOUT
========================================================================

    $8000 ............ JMP COLD_START (type 8000R in WozMon for BASIC)
    $8003-$9EE4 ...... Microsoft BASIC, OSI version 1.0 rev 3.2,
                       6-digit floating point
    $9EE5-$FCFF ...... free, about 24K for your own ROM code
    $FD00 ............ JMP CHRIN
    $FD03 ............ JMP CHROUT
    $FE00 ............ WozMon (the reset routine jumps here)
    $FFEB-$FFF9 ...... the five entry points OSI BASIC expects
                       from its monitor
    $FFFA-$FFFF ...... NMI, RESET and IRQ vectors

========================================================================
 4. BIOS ENTRY POINTS
========================================================================

    $FD00  CHRIN ..... if a key is waiting: read it, echo it, return
                       it in A with C=1. Otherwise C=0.
    $FD03  CHROUT .... send A to the terminal. A, X and Y preserved.
    $FE00  WozMon .... jump here to return to the monitor.

 Your programs can live in RAM from $0300. BASIC also keeps its
 program from $0300 upwards, so running BASIC overwrites them.

 A minimal program that prints HELLO! and returns to WozMon. Type it
 at the WozMon prompt, then run it with 300R:

    0300: A2 00 BD 10 03 F0 06 20 03 FD E8 D0 F5 4C 00 FE
    0310: 48 45 4C 4C 4F 21 0D 00

    0300  A2 00       LDX #$00
    0302  BD 10 03    LDA $0310,X     ; next character
    0305  F0 06       BEQ $030D       ; 0 marks the end
    0307  20 03 FD    JSR $FD03       ; CHROUT
    030A  E8          INX
    030B  D0 F5       BNE $0302
    030D  4C 00 FE    JMP $FE00       ; back to WozMon
    0310  "HELLO!" CR 0

========================================================================
 5. WHAT THE BIOS DOES FOR BASIC
========================================================================

  - A key reader that does not echo. BASIC echoes what you type by
    itself.
  - A Ctrl+C check. Ctrl+C stops a running program with BREAK IN.
  - A 64-key typeahead buffer in page 2 ($0280-$02BF), so keys typed
    while a program runs are not lost.
  - An output routine that clears bit 7. BASIC marks the last letter
    of its error messages that way, and a terminal would show garbage.

 LOAD and SAVE do nothing.

========================================================================
 6. BASIC NOTES
========================================================================

  - At start-up BASIC asks MEMORY SIZE? and TERMINAL WIDTH?. Press
    Return for both, and BASIC finds the RAM by itself (15615 bytes
    free).
  - This is the 1977 dialect: there is no CLS, ELSE or DELETE.
  - To clear the screen, print the ANSI escape codes:

        PRINT CHR$(27);"[2J";CHR$(27);"[H"

  - To delete a line, type its number alone and press Return.
  - Numbers have about 6 significant digits.

========================================================================
 7. BUILDING THE ROM
========================================================================

 You need ca65 and ld65 from cc65 (https://cc65.github.io/), and the
 Microsoft BASIC sources reconstructed in mist64/msbasic
 (https://github.com/mist64/msbasic). From the top of the repository:

    $ git clone https://github.com/mist64/msbasic
    $ cd rom
    $ MSBASIC=../msbasic ./build.sh

 This produces rom.bin, exactly 32768 bytes. Offset 0 of the file is
 $8000.

========================================================================