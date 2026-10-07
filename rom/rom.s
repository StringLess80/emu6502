; rom.s - The 32 KiB ROM of the 6502 board: MS BASIC, a BIOS and WozMon.
;
; Build (see build.sh):
;   ca65 -D osi -I <msbasic folder> rom.s -o rom.o
;   ld65 -C sbc6502.cfg rom.o -o rom.bin

; ---------------------------------------------------------------- BASIC
; msbasic.s is Microsoft BASIC as reconstructed by github.com/mist64/msbasic.
; Assembled with -D osi it is the Ohio Scientific version, which talks to
; its "monitor" through fixed addresses $FFEB-$FFF7: we put our own
; routines there (segment MONJUMPS below).

.include "msbasic.s"

        .segment "BASICENTRY"
        jmp     COLD_START          ; $8000: "8000R" in WozMon

; ---------------------------------------------------------------- BIOS
ACIA_DATA       = $5000
ACIA_STATUS     = $5001
ACIA_CMD        = $5002
ACIA_CTRL       = $5003

VIA_PORTB       = $6000
VIA_PORTA       = $6001
VIA_DDRB        = $6002
VIA_DDRA        = $6003

        .segment "BIOS"

; Fixed entry points for your own programs.
        jmp     CHRIN               ; $FD00
        jmp     CHROUT              ; $FD03

; CHRIN: if a key is waiting, read it, echo it, return it in A with C=1.
; Otherwise return C=0 at once. Same as the old BIOS.
CHRIN:  lda     ACIA_STATUS
        and     #$08                ; RDRF: a byte has arrived?
        beq     @no_key
        lda     ACIA_DATA
        jsr     CHROUT              ; echo
        sec
        rts
@no_key:
        clc
        rts

; CHROUT: send A. A, X and Y are preserved.
CHROUT: sta     ACIA_DATA
        pha
@wait:  lda     ACIA_STATUS
        and     #$10                ; TDRE: ready for the next byte?
        beq     @wait
        pla
        rts

; Keys that arrive while a BASIC program runs (BAS_ISCNTC has to read
; them to see whether they are Ctrl+C) wait in a small ring buffer, so
; typing ahead works. Page 2 is free: BASIC programs start at $0300 and
; WozMon's line buffer ends at $027F.
KEYBUF          = $0280             ; 64 bytes, $0280-$02BF
KEY_HEAD        = $02FE             ; next key to take out
KEY_TAIL        = $02FF             ; next free slot

SAVE_X          = $02FD

; BASIC's "read a key": wait for one and return it in A. X and Y are
; preserved. No echo: BASIC echoes what you type by itself.
BAS_RDKEY:
        stx     SAVE_X
        ldx     KEY_HEAD
        cpx     KEY_TAIL
        beq     @wait               ; buffer empty: ask the ACIA
        lda     KEYBUF,x            ; take the oldest key
        pha
        inx
        txa
        and     #$3F                ; wrap around after 64
        sta     KEY_HEAD
        pla
        ldx     SAVE_X
        rts
@wait:  lda     ACIA_STATUS
        and     #$08                ; RDRF
        beq     @wait
        lda     ACIA_DATA
        ldx     SAVE_X
        rts

; BASIC's "was Ctrl+C pressed?". Called often while a program runs.
; Ctrl+C: enter BASIC's STOP with A=3, C=1 and Z=1, exactly as BASIC's
; own ISCNTC would. Any other key goes into the buffer. X is preserved.
BAS_ISCNTC:
        lda     ACIA_STATUS
        and     #$08
        beq     @done
        lda     ACIA_DATA
        cmp     #$03                ; Ctrl+C sets Z and C
        bne     @keep
        jmp     STOP
@keep:  stx     SAVE_X
        ldx     KEY_TAIL
        sta     KEYBUF,x
        inx
        txa
        and     #$3F
        cmp     KEY_HEAD            ; full? then forget this key
        beq     @full
        sta     KEY_TAIL
@full:  ldx     SAVE_X
@done:  rts

; BASIC's "print a character". BASIC marks the last letter of its error
; messages by setting bit 7, which a serial terminal shows as garbage.
BAS_COUT:
        and     #$7F
        jmp     CHROUT

; LOAD and SAVE are not supported (no tape on this board).
BAS_LOADSAVE:
        rts

; Power-on and reset.
RESET:  cld
        sei
        ldx     #$FF
        txs
        lda     #0
        sta     KEY_HEAD
        sta     KEY_TAIL
        jmp     WOZMON

; There is nothing to interrupt us, but just in case:
IRQ:
NMI:    rti

; ---------------------------------------------------------------- WozMon
        .segment "WOZMON"
.proc WOZMON
        .include "wozmon.s"
.endproc

; ---------------------------------------------------------------- OSI jump table
        .segment "MONJUMPS"
        jmp     BAS_RDKEY           ; $FFEB MONRDKEY
        jmp     BAS_COUT            ; $FFEE MONCOUT
        jmp     BAS_ISCNTC          ; $FFF1 MONISCNTC
        jmp     BAS_LOADSAVE        ; $FFF4 LOAD
        jmp     BAS_LOADSAVE        ; $FFF7 SAVE

        .segment "RESETVEC"
        .word   NMI                 ; $FFFA
        .word   RESET               ; $FFFC
        .word   IRQ                 ; $FFFE
