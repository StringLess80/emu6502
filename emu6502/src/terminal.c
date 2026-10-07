/*
 * terminal.c - Raw keyboard input and screen output, POSIX version
 * (Linux, macOS, WSL on Windows).
 */
#define _POSIX_C_SOURCE 200809L   /* ask for the POSIX functions */

#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

#include "terminal.h"

static struct termios saved;      /* settings before we changed them */
static bool raw_enabled = false;
static bool input_closed = false; /* stdin reached end of file       */

void terminal_restore(void)
{
    if (raw_enabled) {
        fflush(stdout);
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
        raw_enabled = false;
    }
}

bool terminal_init(void)
{
    /* If stdin is not a terminal (for example a file or a pipe:
     * ./emu6502 < keys.txt) there is nothing to configure. */
    if (!isatty(STDIN_FILENO)) {
        return true;
    }

    if (tcgetattr(STDIN_FILENO, &saved) != 0) {
        perror("tcgetattr");
        return false;
    }

    struct termios raw = saved;
    /* Input: no CR->NL translation, no Ctrl+S/Ctrl+Q flow control. */
    raw.c_iflag &= (tcflag_t)~(ICRNL | IXON | BRKINT | INPCK | ISTRIP);
    /* Output: no processing; we send exactly the bytes we want. */
    raw.c_oflag &= (tcflag_t)~(OPOST);
    /* Local: no echo, no line editing, no signals from Ctrl+C/Ctrl+Z. */
    raw.c_lflag &= (tcflag_t)~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cflag |= CS8;
    /* read() returns immediately, even if no byte is available. */
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) != 0) {
        perror("tcsetattr");
        return false;
    }
    raw_enabled = true;
    atexit(terminal_restore);   /* restore even if we exit() early */
    return true;
}

int terminal_get(void)
{
    if (input_closed) {
        return -1;
    }

    /* poll() with a timeout of 0 asks: "is there something to read
     * right now?" without waiting. */
    struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
    if (poll(&pfd, 1, 0) <= 0) {
        return -1;
    }

    unsigned char byte;
    ssize_t n = read(STDIN_FILENO, &byte, 1);
    if (n == 1) {
        return byte;
    }
    if (n == 0 && !isatty(STDIN_FILENO)) {
        input_closed = true;    /* end of a file or pipe */
    }
    return -1;
}

void terminal_put(uint8_t byte)
{
    putchar(byte);
}

void terminal_flush(void)
{
    fflush(stdout);
}
