# Makefile for the 6502 SBC emulator.
#
#   make          build the emulator (./emu6502)
#   make test     build and run the unit tests
#   make clean    delete everything that make built

CC      = cc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -O2 -g
LDFLAGS =

# Every .c file of the emulator, and the .o file made from each one.
SRCS = src/main.c src/cpu.c src/opcodes.c src/sbc.c src/acia.c \
       src/via.c src/terminal.c src/loader.c src/disasm.c
OBJS = $(SRCS:.c=.o)

emu6502: $(OBJS)
	$(CC) $(LDFLAGS) -o $@ $(OBJS)

# How to turn any .c into a .o. $< is the .c file, $@ is the .o file.
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Each object also depends on the headers it includes.
src/main.o:     src/cpu.h src/bus.h src/disasm.h src/loader.h src/sbc.h \
                src/acia.h src/via.h src/terminal.h
src/cpu.o:      src/cpu.h src/bus.h src/opcodes.h
src/opcodes.o:  src/opcodes.h
src/sbc.o:      src/sbc.h src/bus.h src/acia.h src/via.h
src/acia.o:     src/acia.h
src/via.o:      src/via.h
src/terminal.o: src/terminal.h
src/loader.o:   src/loader.h src/sbc.h src/bus.h src/acia.h src/via.h
src/disasm.o:   src/disasm.h src/bus.h src/opcodes.h

# ---- Tests ---------------------------------------------------------

TEST_CPU = src/cpu.c src/opcodes.c tests/flatbus.c

tests/test_cpu: tests/test_cpu.c $(TEST_CPU) src/cpu.h src/opcodes.h tests/flatbus.h
	$(CC) $(CFLAGS) -o $@ tests/test_cpu.c $(TEST_CPU)

tests/functest: tests/functest.c $(TEST_CPU) src/cpu.h tests/flatbus.h
	$(CC) $(CFLAGS) -o $@ tests/functest.c $(TEST_CPU)

test: tests/test_cpu
	./tests/test_cpu

# Needs 6502_functional_test.bin (see the chapter on testing).
functest: tests/functest
	./tests/functest tests/6502_functional_test.bin

clean:
	rm -f emu6502 $(OBJS) tests/test_cpu tests/functest

# These names are commands, not files.
.PHONY: test functest clean
