/* spark -- kernel/vga.h
 *
 * Enough of the screen to report what the kernel is doing: a fixed grid of
 * cells and no cursor, scrolling or formatting.
 */
#ifndef SPARK_VGA_H
#define SPARK_VGA_H

#include "types.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* The colour byte: the low four bits are the foreground, the next three the
 * background, and the top bit makes the character blink. */
#define VGA_BLACK      0
#define VGA_GREY       7
#define VGA_LIGHT_GREY 7
#define VGA_WHITE      15

static inline uint8_t vga_colour(uint8_t foreground,
                                 uint8_t background)
{
    return (uint8_t) (foreground | background << 4);
}

void vga_clear(uint8_t colour);
void vga_write(int row, int column, const char *text, uint8_t colour);

#endif
