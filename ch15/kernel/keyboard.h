/* spark -- kernel/keyboard.h
 *
 * Interrupt-driven input from the 8042 keyboard controller.
 *
 * The screen is always ready, so polling it costs nothing. A keyboard is
 * ready a few times a second while somebody types and not at all otherwise,
 * so polling it spends the whole processor on negative answers. This driver
 * takes IRQ 1 instead: the controller raises it on every key press and
 * release, and the handler leaves the result in a buffer.
 */
#ifndef SPARK_KEYBOARD_H
#define SPARK_KEYBOARD_H

#include "types.h"

/* Data on one port, status and commands on the other. */
#define KBD_DATA   0x60
#define KBD_STATUS 0x64

/* Bit 0 of the status port: a byte is waiting at the data port. A polling
 * driver spins on this bit; an interrupt-driven one never reads it. */
#define KBD_OUTPUT_FULL 0x01

/* Claim IRQ 1 and open the line. */
void kbd_init(void);

/* One character, waiting for one if the buffer is empty. The processor is
 * halted between interrupts while it waits. */
char kbd_getchar(void);

/* Whether a character is buffered, for callers that must not block. */
bool kbd_haschar(void);

/* Read a line into buf, echoing each character and handling backspace, until
 * Enter. Returns the length excluding the terminator, and never writes more
 * than size bytes including it. */
size_t kbd_readline(char *buf, size_t size);

/* Scancodes seen by the handler. A press and its release are two codes, so
 * this runs at roughly twice the number of characters typed. */
uint32_t kbd_scancodes(void);

/* The polling equivalent, retained for measurement: it spins on the status
 * port until a byte appears and adds the number of reads it took to *asked.
 * No part of the kernel uses it to obtain input. */
char kbd_poll_char(uint32_t *asked);

#endif
