#!/bin/sh
# Build rom.bin for the 6502 board.
#   MSBASIC: folder of a clone of https://github.com/mist64/msbasic
#   ca65 and ld65 from cc65 must be on the PATH.
set -e
MSBASIC=${MSBASIC:-../msbasic}
ca65 -D osi -I "$MSBASIC" rom.s -o rom.o
ld65 -C sbc6502.cfg rom.o -o rom.bin -Ln rom.lbl
ls -l rom.bin
