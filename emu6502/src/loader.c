/*
 * loader.c - Reading binary and text files into memory.
 */
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#include "loader.h"

bool load_binary(Bus *sbc, const char *path, uint16_t addr)
{
    FILE *f = fopen(path, "rb");      /* "rb" = read, binary */
    if (f == NULL) {
        perror(path);                 /* prints e.g. "x.bin: No such file" */
        return false;
    }

    int c;
    long count = 0;
    while ((c = fgetc(f)) != EOF) {
        if (count > 0xFFFF) {
            fprintf(stderr, "%s: larger than the address space\n", path);
            fclose(f);
            return false;
        }
        sbc_load_byte(sbc, (uint16_t)(addr + count), (uint8_t)c);
        count++;
    }

    bool ok = !ferror(f);
    if (!ok) {
        perror(path);
    }
    fclose(f);
    return ok;
}

bool load_rom(Bus *sbc, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        perror(path);
        return false;
    }

    /* Find out the size: jump to the end and ask where we are. */
    if (fseek(f, 0, SEEK_END) != 0) {
        perror(path);
        fclose(f);
        return false;
    }
    long size = ftell(f);
    fclose(f);

    if (size <= 0 || size > ROM_SIZE) {
        fprintf(stderr, "%s: a ROM image must be 1 to %d bytes (this one is %ld)\n",
                path, ROM_SIZE, size);
        return false;
    }
    /* A smaller image goes at the top of the ROM, so that its last
     * bytes (the vectors) end at the top of memory. */
    uint32_t start = ROM_START + ROM_SIZE - (uint32_t)size;
    return load_binary(sbc, path, (uint16_t)start);
}

/* Parse one line of WozMon text. Returns false on a syntax error. */
static bool parse_hex_line(Bus *sbc, const char *line)
{
    char *end;

    /* Skip leading spaces; ignore empty lines and comments. */
    while (isspace((unsigned char)*line)) {
        line++;
    }
    if (*line == '\0' || *line == ';' || *line == '#') {
        return true;
    }

    /* The address, then a colon. */
    unsigned long addr = strtoul(line, &end, 16);
    if (end == line || *end != ':') {
        return false;
    }
    line = end + 1;

    /* Then any number of hex bytes separated by spaces. */
    for (;;) {
        while (isspace((unsigned char)*line)) {
            line++;
        }
        if (*line == '\0') {
            return true;
        }
        unsigned long byte = strtoul(line, &end, 16);
        if (end == line || byte > 0xFF) {
            return false;
        }
        sbc_load_byte(sbc, (uint16_t)addr, (uint8_t)byte);
        addr++;
        line = end;
    }
}

bool load_wozhex(Bus *sbc, const char *path)
{
    FILE *f = fopen(path, "r");       /* text mode */
    if (f == NULL) {
        perror(path);
        return false;
    }

    char line[256];
    int line_number = 0;
    bool ok = true;

    while (fgets(line, sizeof line, f) != NULL) {
        line_number++;
        if (!parse_hex_line(sbc, line)) {
            fprintf(stderr, "%s:%d: cannot understand this line\n",
                    path, line_number);
            ok = false;
            break;
        }
    }

    fclose(f);
    return ok;
}
