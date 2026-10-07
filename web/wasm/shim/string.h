/*
 * string.h - The tiny part of the C library the emulator core needs when
 * it is compiled to WebAssembly without a C library. emu_wasm.c
 * implements these functions.
 */
#ifndef SHIM_STRING_H
#define SHIM_STRING_H

#include <stddef.h>

void *memset(void *dst, int value, size_t n);
void *memcpy(void *dst, const void *src, size_t n);

#endif
