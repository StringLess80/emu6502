[![emu6502 running Microsoft BASIC in the web interface](docs/images/emu6502.png)](https://stringless80.github.io/emu6502/)

[Run it in your browser](https://stringless80.github.io/emu6502/) -
[docs/rom.md](docs/rom.md) -
[LICENSE](LICENSE)

```text
  _____ __  __ _   _  __  ____   ___ ____
 | ____|  \/  | | | |/ /_| ___| / _ \___ \
 |  _| | |\/| | | | | '_ \___ \| | | |__) |
 | |___| |  | | |_| | (_) |__) | |_| / __/
 |_____|_|  |_|\___/ \___/____/ \___/_____|

 A 6502 single-board computer emulator, written in C.

========================================================================

 CONTENTS

    1.  Introduction
    2.  Demonstration
    3.  The emulated system
    4.  Building
    5.  Usage
    6.  The web interface
    7.  Architecture
    8.  Files
    9.  Testing
    10. Known limitations
    11. License

========================================================================
 1. INTRODUCTION
========================================================================

 emu6502 emulates a small 6502 computer: the CPU, 16K of RAM, 32K of
 ROM, a 6551 ACIA serial port and a 6522 VIA. It runs at 1 MHz in a
 terminal, and the included ROM boots WozMon and Microsoft BASIC.

 The same C core also runs in a web browser, compiled to WebAssembly,
 with a debugger around it.

    Language ......... C11 + POSIX, no libraries beyond libc
    CPU .............. NMOS 6502, all 256 opcodes
    Memory ........... 16K RAM, 32K ROM, ACIA at $5000, VIA at $6000
    Timing ........... 1 MHz in real time; any clock, or unlimited
    Debugging ........ instruction trace, VIA port display
    ROM .............. MS BASIC + BIOS + WozMon, built with cc65
    Web .............. https://stringless80.github.io/emu6502/

 NOTE: the emulator is accurate to the instruction, not to the cycle.
 Each instruction takes the right number of cycles, but the dummy reads
 and double writes inside NMOS instructions are not reproduced. Nothing
 in the emulated system depends on them (see section 10).

========================================================================
 2. DEMONSTRATION
========================================================================

 Microsoft BASIC, started from WozMon:

    $ ./emu6502 --rom rom.bin
    [6502 SBC emulator - Ctrl+] quits, Ctrl+R resets]
    \
    8000R

    8000: 4C
    MEMORY SIZE?
    TERMINAL WIDTH?

     15615 BYTES FREE

    OSI 6502 BASIC VERSION 1.0 REV 3.2
    COPYRIGHT 1977 BY MICROSOFT CO.

    OK
    10 PRINT MID$("/\",RND(1)*2+1,1);:GOTO 10
    RUN
    \/\\\///\/\\//\/\\\//\\\//\\//\/\//\\\/\\\\\/\\\/\/\/////\/\\\\\/
    ///\//\\/\/\\\\//\//\\\////\/\\\\/\/\\/\//\/\///////\//\\\\\\///

 A machine-language program typed into WozMon, printing through the
 BIOS routine CHROUT at $FD03:

    \
    0300: A2 00 BD 10 03 F0 06 20 03 FD E8 D0 F5 4C 00 FE
    0310: 48 45 4C 4C 4F 21 0D 00
    300R

    0300: A2HELLO!
    \

========================================================================
 3. THE EMULATED SYSTEM
========================================================================

 3.1 Memory map

    +---------------+-------------+-----------------------------------+
    | Address       | Chip        | Notes                             |
    +---------------+-------------+-----------------------------------+
    | $0000-$3FFF   | RAM, 16K    | zero page, stack, then free RAM   |
    | $4000-$4FFF   | -           | free, reads $FF                   |
    | $5000-$5FFF   | ACIA 6551   | 4 registers, mirrored             |
    | $6000-$6FFF   | VIA 6522    | 16 registers, mirrored            |
    | $7000-$7FFF   | -           | free, reads $FF                   |
    | $8000-$FFFF   | ROM, 32K    | writes from the CPU are ignored   |
    +---------------+-------------+-----------------------------------+

 Each I/O chip answers anywhere in its 4K slot: the ACIA's 4 registers
 and the VIA's 16 repeat across the whole window. The register list is
 in docs/rom.md.

 3.2 CPU

  - All 256 NMOS 6502 opcodes: the 151 documented ones in all 13
    addressing modes, and the 105 undocumented ones.
  - Undocumented opcodes do what the real chip does: SLO RLA SRE RRA
    DCP ISC LAX SAX ANC ALR ARR SBX SBC# ANE LXA LAS SHA SHX SHY TAS,
    and NOPs of 1 to 3 bytes that still read their operand. ANE and
    LXA use the common magic constant $EE.
  - The 12 JAM opcodes lock the CPU up like the real chip. The emulator
    reports where, and Reset brings it back.
  - Decimal mode for ADC and SBC, with the NMOS flag behaviour.
  - The JMP ($xxFF) page-wrap bug, zero-page wrap-around, and the B
    flag only in pushed copies of P.
  - Cycle counts include +1 for page crossings on indexed reads and
    +1/+2 for taken branches.

 3.3 6551 ACIA

    Data ............. read: last byte received, clears RDRF
                       write: sent to the terminal at once
    Status ........... TDRE always 1; RDRF set when a byte is waiting
    Command/Control .. stored; programmed reset on writes to Status

 Keys wait in a 256-byte queue and are handed to the ACIA one at a
 time, whenever its receive register is empty. Whole programs can be
 pasted into WozMon without losing a character.

 3.4 6522 VIA

  - Ports A and B with their data direction registers. Inputs read 1,
    like unconnected pins.
  - Timer 1 in one-shot and free-run mode; timer 2 in one-shot mode.
    The IFR flags are cleared by reading the counter or writing IFR.
  - --ports shows the pins whenever they change: '#' is an output at 1,
    '.' an output at 0, '-' an input.

 3.5 ROM

    $8000 ............ JMP COLD_START  (type 8000R in WozMon for BASIC)
    $8003-$9EE4 ...... Microsoft BASIC (OSI 1.0 rev 3.2)
    $9EE5-$FCFF ...... free, about 24K
    $FD00 / $FD03 .... BIOS CHRIN / CHROUT
    $FE00 ............ WozMon
    $FFFA-$FFFF ...... NMI, RESET and IRQ vectors

========================================================================
 4. BUILDING
========================================================================

 4.1 The emulator

 You need a C11 compiler (gcc or clang) and make. On Windows, use WSL.

    $ git clone https://github.com/StringLess80/emu6502.git
    $ cd emu6502/emu6502
    $ make

 Without make:

    $ gcc -std=c11 -Wall -Wextra -Wpedantic -O2 -o emu6502 src/*.c

 4.2 The ROM

 The ROM is 6502 assembly, built with ca65/ld65 from cc65
 (https://cc65.github.io/, "apt install cc65" or "brew install cc65"),
 using the Microsoft BASIC sources reconstructed in mist64/msbasic.
 From the top of the repository:

    $ git clone https://github.com/mist64/msbasic
    $ cd rom
    $ MSBASIC=../msbasic ./build.sh        # makes rom.bin, 32768 bytes

 Offset 0 of rom.bin is $8000.

 4.3 Running

    $ ../emu6502/emu6502 --rom rom.bin

 WozMon prints '\'. Type 8000R to start BASIC, and press Return at the
 two questions.

========================================================================
 5. USAGE
========================================================================

    emu6502 --rom FILE [options]

    --rom FILE ........ ROM image, up to 32K, placed to end at $FFFF
    --load FILE@ADDR .. load a raw binary at a hex address, e.g.
                        prog.bin@0300 (may be repeated)
    --hex FILE ........ load a WozMon text file, lines like
                        "0300: A9 01 85 00" (may be repeated)
    --clock HZ ........ CPU clock in Hz; default 1000000, 0 = no limit
    --trace FILE ...... write every executed instruction to FILE
    --ports ........... show the VIA port pins whenever they change
    --no-upcase ....... do not turn typed letters into capitals
    --max-cycles N .... stop after N cycles (for scripts)

 Keys while running:

    Ctrl+] ............ quit, and print the CPU registers
    Ctrl+R ............ reset button: reset CPU and chips, keep RAM
    Ctrl+C ............ passed to the 6502; stops a BASIC program

 The terminal sends CR for Return and BS for Backspace. CR in the
 output becomes CR LF.

 5.1 Trace format

 One line per instruction, with the registers just before it runs:

    FD7D  CLD            A=00 X=00 Y=00 SP=FD P=..-..I.. CYC=7
    FD7E  SEI            A=00 X=00 Y=00 SP=FD P=..-..I.. CYC=9
    FD7F  LDX #$FF       A=00 X=00 Y=00 SP=FD P=..-..I.. CYC=11
    FD81  TXS            A=00 X=FF Y=00 SP=FD P=N.-..I.. CYC=13

 It is plain text, so grep, awk and diff work on it.

 5.2 Scripted runs

 When standard input is not a terminal, keys are read from it. With
 --clock 0 and --max-cycles this makes regression tests easy:

    $ printf '8000R\r\r\rPRINT 2^10\r' | ./emu6502 --rom rom.bin \
          --clock 0 --max-cycles 3000000

========================================================================
 6. THE WEB INTERFACE
========================================================================

 The web/ folder runs the emulator in a browser. It is not a rewrite:
 web/wasm/build.sh compiles the same cpu.c, sbc.c, acia.c, via.c and
 opcodes.c to WebAssembly, and JavaScript draws everything around them.

 Online:  https://stringless80.github.io/emu6502/
          It boots into WozMon; type 8000R for BASIC.

 Panels:

    Terminal ......... the ACIA as an 80x24 screen; paste works, Ctrl+C
                       goes to the 6502. On phones, tap it to type;
                       buttons below send Ctrl+C, Esc, Tab, Return
    CPU .............. registers and flags, changed values lit, cycle
                       count and measured clock speed
    Disassembly ...... code around PC; click a line for a breakpoint
    Stack ............ page 1 from SP up, JSR return addresses marked
    Memory ........... 256 bytes in hex and ASCII; click to edit
    I/O .............. VIA port pins as LEDs, timers, flags, ACIA
    Top bar .......... a copy of the port A and B LEDs

 Controls:

    Run/Pause (F9) ... 100 kHz, 1, 2 or 4 MHz, or unlimited
    Step (F7) ........ one instruction
    Step over (F8) ... a whole JSR
    Reset ............ reset CPU and chips, keep RAM
    Power cycle ...... clear RAM and reload the ROM
    Load ROM ......... a ROM image, remembered for next time
    Load program ..... a WozMon .hex file, or a .bin at the address
                       given next to the button

 Files can also be dropped on the page.

 Running it from disk: open web/index.html and load rom.bin with Load
 ROM. From a local server (python3 -m http.server in web/), a rom.bin
 next to index.html loads by itself.

 Publishing: .github/workflows/pages.yml runs on every push to main.
 It compiles the WebAssembly core, assembles rom.bin with cc65 and
 publishes the site on GitHub Pages. To rebuild the core by hand you
 need clang and wasm-ld, nothing else:

    $ web/wasm/build.sh                     # writes web/emu-wasm.js

========================================================================
 7. ARCHITECTURE
========================================================================

 The CPU knows nothing about the rest of the system. It reads and
 writes through the three functions of bus.h. The same cpu.c is linked
 with sbc.c in the emulator, and with a flat 64K test bus in the tests.

                       +-----------+
                       |  main.c   |  options, main loop, clock
                       +-----------+
                        |    |    |
              cpu_step  |    |    |  --rom --load --hex
           +------------+    |    +-------------+
           v                 |                  v
     +-----------+           |            +-----------+
     |   cpu.c   |           |            | loader.c  |
     +-----------+           |            +-----------+
           ^ bus_read        |                  |
           v bus_write       v                  |
     +-----------+     +------------+           |
     |   sbc.c   |<----| terminal.c |           |
     |  decoder  |     |  raw mode  |           |
     +-----------+     +------------+           |
       |   |   |              ^                 |
       |   |   +--> acia.c ---+ bytes           |
       |   +------> via.c                       |
       +----------> RAM / ROM <-----------------+

 The main loop runs the CPU in slices of 1 ms of emulated time. After
 each slice it moves bytes between the ACIA and the terminal, and
 sleeps until real time catches up. The VIA timers advance after every
 instruction.

========================================================================
 8. FILES
========================================================================

    emu6502/
      Makefile
      src/
        main.c ............... options, main loop, key queue, trace
        cpu.c, cpu.h ......... the 6502 core
        opcodes.c, .h ........ name, mode and cycles of all 256 opcodes
        bus.h ................ the CPU's only view of the world
        sbc.c, sbc.h ......... memory map and address decoder
        acia.c, acia.h ....... 6551 ACIA
        via.c, via.h ......... 6522 VIA
        terminal.c, .h ....... raw-mode keyboard and screen (termios)
        loader.c, .h ......... ROM, binary and WozMon-text loaders
        disasm.c, .h ......... one-instruction disassembler
      tests/
        test_cpu.c ........... unit tests for single instructions
        functest.c ........... runner for Klaus Dormann's test
        singlestep.c, .py .... runner for the SingleStepTests vectors
        flatbus.c, .h ........ 64K flat-RAM bus for the tests
    web/
      index.html ............. the web interface
      app.js, style.css ...... terminal, panels, keyboard, files
      emu-wasm.js ............ the C core as WebAssembly (generated)
      fonts/ ................. VT323 and IBM Plex, self-hosted (OFL)
      wasm/emu_wasm.c ........ exports the core to JavaScript
      wasm/build.sh .......... builds emu-wasm.js with clang
    rom/
      rom.s .................. MS BASIC + BIOS + jump tables + vectors
      wozmon.s ............... WozMon
      sbc6502.cfg ............ ld65 memory layout
      build.sh ............... builds rom.bin
    docs/
      rom.md ................. memory map, registers, ROM, BIOS
      images/ ................ screenshot
    .github/workflows/
      pages.yml .............. builds and publishes the web emulator

========================================================================
 9. TESTING
========================================================================

    $ make test         unit tests: instructions, flags, stack, cycles
    $ make functest     Klaus Dormann's 6502 functional test
    $ make singlestep SST=path/to/6502/v1

 make functest needs 6502_functional_test.bin from
 github.com/Klaus2m5/6502_65C02_functional_tests (bin_files/), copied
 into tests/. It exercises every documented instruction and flag,
 decimal mode included, and traps at a known address on success:

    PASS: stopped at $3469 after 96241367 cycles

 make singlestep runs the vectors of github.com/SingleStepTests/65x02
 (folder 6502/v1, files 00.json to ff.json): 10,000 cases per opcode
 recorded from a real NMOS 6502, each checking registers, flags, memory
 and cycle count after one instruction. All 244 non-JAM opcodes pass
 every case, undocumented ones included. The JAM opcodes are skipped,
 since the vectors record bus activity of the locked-up chip.

 Both suites also pass under -fsanitize=address,undefined.

========================================================================
 10. KNOWN LIMITATIONS
========================================================================

  1. No device interrupts. The ACIA and VIA never raise IRQ, and
     nothing raises NMI; BRK and RTI work. Interrupt-driven code will
     not run.

  2. Instruction-level timing. No dummy reads or double writes inside
     instructions; the VIA timers advance once per instruction. I/O
     registers that react to those extra accesses would behave
     differently. The emulated ACIA and VIA do not.

  3. Instant serial transmit. No baud-rate timing; TDRE is always set.

  4. Partial 6522. The shift register, the handshake lines (CA/CB) and
     timer 2 pulse counting are stored but have no effect.

  5. ANE and LXA vary between real chips; the emulator always uses $EE.

  6. POSIX only. The terminal code uses termios and poll. Windows needs
     WSL, or a port of terminal.c.

  7. No LOAD or SAVE in the BASIC ROM. Programs are lost on reset.

========================================================================
 11. LICENSE
========================================================================

 emu6502 is free software, released under the GNU General Public
 License version 3. See the file LICENSE.

 WozMon is (c) Steve Wozniak / Apple, included for educational use.
 Microsoft BASIC is not stored in this repository: it is fetched from
 mist64/msbasic when the ROM is built. The fonts in web/fonts/ are
 under the SIL Open Font License.

========================================================================
```