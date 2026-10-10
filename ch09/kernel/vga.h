/* spark -- kernel/vga.h
 *
 * Text-mode screen output: an 80 by 25 grid of character cells.
 */
#ifndef SPARK_VGA_H
#define SPARK_VGA_H

#include "types.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* The sixteen colours, in the order the hardware numbers them. Only the first
 * eight are valid as a background: that field is three bits wide, and the
 * eighth bit of the attribute byte is the blink flag. */
#define VGA_BLACK          0
#define VGA_BLUE           1
#define VGA_GREEN          2
#define VGA_CYAN           3
#define VGA_RED            4
#define VGA_MAGENTA        5
#define VGA_BROWN          6
#define VGA_LIGHT_GREY     7
#define VGA_DARK_GREY      8
#define VGA_LIGHT_BLUE     9
#define VGA_LIGHT_GREEN   10
#define VGA_LIGHT_CYAN    11
#define VGA_LIGHT_RED     12
#define VGA_LIGHT_MAGENTA 13
#define VGA_YELLOW        14
#define VGA_WHITE         15

static inline uint8_t vga_colour(uint8_t foreground,
                                 uint8_t background)
{
    return (uint8_t) (foreground | background << 4);
}

void vga_init(void);
void vga_clear(void);
void vga_set_colour(uint8_t colour);
void vga_putchar(char c);
void vga_backspace(void);
void vga_write(const char *text);

#endif
