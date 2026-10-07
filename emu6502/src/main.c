/*
 * main.c - The emulator program: options, the main loop, the keyboard,
 * the screen and the clock.
 */
#define _POSIX_C_SOURCE 200809L   /* for clock_gettime and nanosleep */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cpu.h"
#include "disasm.h"
#include "loader.h"
#include "sbc.h"
#include "terminal.h"

#define KEY_QUIT  0x1D   /* Ctrl+]  */
#define KEY_RESET 0x12   /* Ctrl+R  */

#define MAX_LOADS 16

/* ------------------------------------------------------------------ */
/* Command line options                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    const char *path;
    bool        is_hex;    /* WozMon text file?                  */
    uint16_t    addr;      /* where to put a binary file         */
} LoadRequest;

typedef struct {
    const char *rom_path;
    LoadRequest loads[MAX_LOADS];
    int         load_count;
    long        clock_hz;      /* 0 = as fast as possible   */
    const char *trace_path;    /* NULL = no trace           */
    bool        show_ports;
    bool        upcase;
    uint64_t    max_cycles;    /* 0 = run until Ctrl+]      */
} Options;

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s --rom FILE [options]\n"
        "\n"
        "  --rom FILE         ROM image, up to 32 KiB, ending at $FFFF\n"
        "  --load FILE@ADDR   load a binary file at a hex address (e.g. hello.bin@0500)\n"
        "  --hex FILE         load a WozMon text file (lines like '0500: A9 01 ...')\n"
        "  --clock HZ         CPU clock in Hz (default 1000000, 0 = unlimited)\n"
        "  --trace FILE       write every executed instruction to FILE\n"
        "  --ports            show the VIA port pins whenever they change\n"
        "  --no-upcase        do not turn typed letters into capitals\n"
        "  --max-cycles N     stop after N cycles (useful for scripts)\n"
        "\n"
        "While running: Ctrl+] quits, Ctrl+R presses the reset button.\n",
        prog);
}

/* Parse a hexadecimal number like "0500", "$0500" or "0x0500". */
static bool parse_hex16(const char *text, uint16_t *out)
{
    if (*text == '$') {
        text++;
    }
    char *end;
    unsigned long value = strtoul(text, &end, 16);
    if (end == text || *end != '\0' || value > 0xFFFF) {
        return false;
    }
    *out = (uint16_t)value;
    return true;
}

static bool parse_options(int argc, char *argv[], Options *opt)
{
    /* Defaults */
    memset(opt, 0, sizeof *opt);
    opt->clock_hz = 1000000;
    opt->upcase = true;

    for (int i = 1; i < argc; i++) {
        const char *arg = argv[i];
        /* Options that need a value take it from the next word. */
        bool has_value = (i + 1 < argc);

        if (strcmp(arg, "--rom") == 0 && has_value) {
            opt->rom_path = argv[++i];
        } else if ((strcmp(arg, "--load") == 0 || strcmp(arg, "--hex") == 0)
                   && has_value) {
            if (opt->load_count == MAX_LOADS) {
                fprintf(stderr, "too many files (max %d)\n", MAX_LOADS);
                return false;
            }
            LoadRequest *req = &opt->loads[opt->load_count++];
            req->path = argv[++i];
            req->is_hex = (strcmp(arg, "--hex") == 0);
            req->addr = 0;
            if (!req->is_hex) {
                /* Split "file.bin@0500" at the last '@'. */
                char *at = strrchr(argv[i], '@');
                if (at == NULL || !parse_hex16(at + 1, &req->addr)) {
                    fprintf(stderr, "--load needs FILE@ADDR, e.g. prog.bin@0500\n");
                    return false;
                }
                *at = '\0';   /* cut the string: path ends here */
            }
        } else if (strcmp(arg, "--clock") == 0 && has_value) {
            char *end;
            opt->clock_hz = strtol(argv[++i], &end, 10);
            if (*end != '\0' || opt->clock_hz < 0) {
                fprintf(stderr, "--clock needs a number of Hz\n");
                return false;
            }
        } else if (strcmp(arg, "--trace") == 0 && has_value) {
            opt->trace_path = argv[++i];
        } else if (strcmp(arg, "--ports") == 0) {
            opt->show_ports = true;
        } else if (strcmp(arg, "--no-upcase") == 0) {
            opt->upcase = false;
        } else if (strcmp(arg, "--max-cycles") == 0 && has_value) {
            opt->max_cycles = strtoull(argv[++i], NULL, 10);
        } else {
            fprintf(stderr, "unknown or incomplete option: %s\n", arg);
            return false;
        }
    }

    if (opt->rom_path == NULL) {
        fprintf(stderr, "you must give a ROM image with --rom\n");
        return false;
    }
    return true;
}

/* ------------------------------------------------------------------ */
/* Keyboard queue                                                      */
/* ------------------------------------------------------------------ */

/* Keys typed (or pasted) faster than the 6502 reads them wait here.
 * It is a "ring buffer": head and tail chase each other around the
 * array, wrapping back to 0 at the end. */
#define KEYQ_SIZE 256

typedef struct {
    uint8_t data[KEYQ_SIZE];
    int     head;    /* next key to take out */
    int     tail;    /* next free slot       */
    int     count;
} KeyQueue;

static bool keyq_push(KeyQueue *q, uint8_t key)
{
    if (q->count == KEYQ_SIZE) {
        return false;
    }
    q->data[q->tail] = key;
    q->tail = (q->tail + 1) % KEYQ_SIZE;
    q->count++;
    return true;
}

static bool keyq_pop(KeyQueue *q, uint8_t *key)
{
    if (q->count == 0) {
        return false;
    }
    *key = q->data[q->head];
    q->head = (q->head + 1) % KEYQ_SIZE;
    q->count--;
    return true;
}

/* Make a modern keyboard look like the terminal WozMon expects. */
static int translate_key(int key, bool upcase)
{
    if (key == 0x7F) {
        return 0x08;                 /* Backspace key sends DEL; we want BS */
    }
    if (key == '\n') {
        return '\r';                 /* Enter must be CR ($0D)              */
    }
    if (upcase && key >= 'a' && key <= 'z') {
        return key - 'a' + 'A';      /* WozMon only knows capitals          */
    }
    return key;
}

/* ------------------------------------------------------------------ */
/* Screen                                                              */
/* ------------------------------------------------------------------ */

/* What the screen looks like right now. "static" at file level: these
 * two variables are private to main.c and keep their values between
 * calls. */
static bool last_was_cr = false;     /* the previous byte was a CR    */
static bool at_line_start = true;    /* the cursor is in column 0     */

/* WozMon ends lines with CR only, other programs with CR LF. A modern
 * terminal needs both, so we print CR as CR LF and drop an LF that
 * comes right after a CR. */
static void screen_put(uint8_t byte)
{
    if (byte == '\r') {
        terminal_put('\r');
        terminal_put('\n');
        last_was_cr = true;
        at_line_start = true;
        return;
    }
    if (byte == '\n') {
        if (!last_was_cr) {
            terminal_put('\r');
            terminal_put('\n');
        }
        last_was_cr = false;
        at_line_start = true;
        return;
    }
    last_was_cr = false;
    at_line_start = false;
    terminal_put(byte);
}

/* Print a message from the emulator itself. A message that starts with
 * CR goes on a line of its own, but if the cursor is already at the
 * start of a line we skip that CR, to avoid an empty line. */
static void screen_message(const char *text)
{
    if (*text == '\r' && at_line_start) {
        text++;
    }
    for (const char *p = text; *p != '\0'; p++) {
        screen_put((uint8_t)*p);
    }
}

/* Show the eight pins of a port: # = output high, . = output low,
 * - = input. Bit 7 on the left, like in the datasheet. */
static void format_port(char out[9], uint8_t pins, uint8_t ddr)
{
    for (int bit = 7; bit >= 0; bit--) {
        uint8_t mask = (uint8_t)(1u << bit);
        char c;
        if (!(ddr & mask)) {
            c = '-';
        } else if (pins & mask) {
            c = '#';
        } else {
            c = '.';
        }
        out[7 - bit] = c;
    }
    out[8] = '\0';
}

static void show_ports(const Via *via)
{
    char a[9], b[9], line[64];
    format_port(a, via_port_a(via), via->ddra);
    format_port(b, via_port_b(via), via->ddrb);
    snprintf(line, sizeof line, "\r[VIA] PA %s  PB %s\r", a, b);
    screen_message(line);
}

/* ------------------------------------------------------------------ */
/* Trace                                                               */
/* ------------------------------------------------------------------ */

static void trace_instruction(FILE *out, Cpu *cpu)
{
    char text[32];
    char flags[9];
    const char *names = "NV-BDIZC";

    disasm(cpu->bus, cpu->pc, text, sizeof text);
    for (int i = 0; i < 8; i++) {
        flags[i] = (cpu->p & (0x80 >> i)) ? names[i] : '.';
    }
    flags[8] = '\0';

    fprintf(out, "%04X  %-14s A=%02X X=%02X Y=%02X SP=%02X P=%s CYC=%llu\n",
            cpu->pc, text, cpu->a, cpu->x, cpu->y, cpu->sp,
            flags, (unsigned long long)cpu->cycles);
}

/* ------------------------------------------------------------------ */
/* Time                                                                */
/* ------------------------------------------------------------------ */

static uint64_t now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000u + (uint64_t)ts.tv_nsec;
}

static void sleep_ns(uint64_t ns)
{
    struct timespec ts;
    ts.tv_sec = (time_t)(ns / 1000000000u);
    ts.tv_nsec = (long)(ns % 1000000000u);
    nanosleep(&ts, NULL);
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */

int main(int argc, char *argv[])
{
    Options opt;
    if (!parse_options(argc, argv, &opt)) {
        usage(argv[0]);
        return EXIT_FAILURE;
    }

    /* The whole computer. "static" puts it in global memory instead
     * of on the stack, and guarantees it starts filled with zeros. */
    static Bus sbc;
    sbc_init(&sbc);

    if (!load_rom(&sbc, opt.rom_path)) {
        return EXIT_FAILURE;
    }
    for (int i = 0; i < opt.load_count; i++) {
        const LoadRequest *req = &opt.loads[i];
        bool ok = req->is_hex ? load_wozhex(&sbc, req->path)
                              : load_binary(&sbc, req->path, req->addr);
        if (!ok) {
            return EXIT_FAILURE;
        }
    }

    FILE *trace = NULL;
    if (opt.trace_path != NULL) {
        trace = fopen(opt.trace_path, "w");
        if (trace == NULL) {
            perror(opt.trace_path);
            return EXIT_FAILURE;
        }
    }

    if (!terminal_init()) {
        return EXIT_FAILURE;
    }

    Cpu cpu;
    cpu_init(&cpu, &sbc);
    cpu_reset(&cpu);

    screen_message("[6502 SBC emulator - Ctrl+] quits, Ctrl+R resets]\r");

    /* We run the CPU in slices of 1 ms of emulated time. After each
     * slice we look at the keyboard and, if we are ahead of the real
     * clock, sleep. */
    uint64_t slice = (opt.clock_hz > 0) ? (uint64_t)opt.clock_hz / 1000 : 10000;
    if (slice == 0) {
        slice = 1;
    }
    /* How long one slice takes on the real chip, in nanoseconds. */
    uint64_t slice_ns = (opt.clock_hz > 0)
                      ? slice * 1000000000u / (uint64_t)opt.clock_hz : 0;
    uint64_t deadline = now_ns();
    KeyQueue keys = {0};
    uint8_t last_pa = via_port_a(&sbc.via), last_pb = via_port_b(&sbc.via);
    bool running = true;

    while (running) {
        /* 1. Run one slice of instructions. */
        uint64_t slice_end = cpu.cycles + slice;
        while (cpu.cycles < slice_end && !cpu.halted) {
            if (trace != NULL) {
                trace_instruction(trace, &cpu);
            }
            int used = cpu_step(&cpu);
            via_tick(&sbc.via, used);

            uint8_t out;
            if (acia_transmit(&sbc.acia, &out)) {
                screen_put(out);
            }
        }

        if (cpu.halted) {
            char msg[80];
            snprintf(msg, sizeof msg,
                     "\r[CPU halted: illegal opcode $%02X at $%04X]\r",
                     bus_peek(&sbc, cpu.pc), cpu.pc);
            screen_message(msg);
            running = false;
        }

        /* 2. Collect keys from the host keyboard. */
        int key;
        while (keys.count < KEYQ_SIZE && (key = terminal_get()) >= 0) {
            if (key == KEY_QUIT) {
                running = false;
            } else if (key == KEY_RESET) {
                sbc_reset_chips(&sbc);
                cpu_reset(&cpu);
                keys.count = keys.head = keys.tail = 0;
                screen_message("\r[RESET]\r");
            } else {
                keyq_push(&keys, (uint8_t)translate_key(key, opt.upcase));
            }
        }

        /* 3. Give the ACIA the next key, if it has room for it. */
        if (!sbc.acia.rx_full) {
            uint8_t next;
            if (keyq_pop(&keys, &next)) {
                acia_receive(&sbc.acia, next);
            }
        }

        /* 4. Show the VIA ports if asked and if they changed. */
        if (opt.show_ports) {
            uint8_t pa = via_port_a(&sbc.via), pb = via_port_b(&sbc.via);
            if (pa != last_pa || pb != last_pb) {
                show_ports(&sbc.via);
                last_pa = pa;
                last_pb = pb;
            }
        }

        terminal_flush();

        if (opt.max_cycles != 0 && cpu.cycles >= opt.max_cycles) {
            running = false;
        }

        /* 5. Keep in step with real time. */
        if (opt.clock_hz > 0) {
            deadline += slice_ns;
            uint64_t now = now_ns();
            if (now < deadline) {
                sleep_ns(deadline - now);
            } else if (now - deadline > 100000000) {
                deadline = now;                  /* >100 ms behind: give up */
            }
        }
    }

    terminal_restore();
    if (trace != NULL) {
        fclose(trace);
    }
    fprintf(stderr,
            "\nPC=%04X A=%02X X=%02X Y=%02X SP=%02X P=%02X cycles=%llu\n",
            cpu.pc, cpu.a, cpu.x, cpu.y, cpu.sp, cpu.p,
            (unsigned long long)cpu.cycles);
    return EXIT_SUCCESS;
}
