/*
 * terminal.h - Use the terminal of the computer we run on (the "host")
 * as the serial terminal connected to the SBC.
 */
#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stdint.h>

/* Put the terminal in "raw" mode: every key arrives immediately,
 * nothing is echoed by the host, Ctrl+C does not kill us.
 * The old settings are restored automatically when the program exits. */
bool terminal_init(void);

/* Restore the original settings (safe to call more than once). */
void terminal_restore(void);

/* Return the next key pressed, or -1 if there is none. Never waits. */
int terminal_get(void);

/* Send one byte to the screen (buffered: call terminal_flush). */
void terminal_put(uint8_t byte);
void terminal_flush(void);

#endif /* TERMINAL_H */
