/*
 * flatbus.h - A test bus with 64 KiB of plain RAM.
 */
#ifndef FLATBUS_H
#define FLATBUS_H

#include <stdint.h>

#include "../src/bus.h"

struct Bus {
    uint8_t mem[65536];
};

#endif /* FLATBUS_H */
