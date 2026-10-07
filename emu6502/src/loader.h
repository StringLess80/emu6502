/*
 * loader.h - Put files into the emulated memory.
 */
#ifndef LOADER_H
#define LOADER_H

#include <stdbool.h>
#include <stdint.h>

#include "sbc.h"

/* Load a ROM image (up to 32 KiB) so that it ends at $FFFF: a full
 * image fills $8000-$FFFF, a smaller one sits at the top with its
 * vectors in place. */
bool load_rom(Bus *sbc, const char *path);

/* Load a raw binary file starting at address addr. */
bool load_binary(Bus *sbc, const char *path, uint16_t addr);

/* Load a text file in WozMon format, lines like:
 *     0500: A2 00 BD 11 05 F0 07
 * (the same text you would paste into WozMon). */
bool load_wozhex(Bus *sbc, const char *path);

#endif /* LOADER_H */
