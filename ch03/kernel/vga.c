/* spark -- kernel/vga.c
 *
 * Direct writes to the text buffer: no cursor, no scrolling, and a bounds
 * check so that a long string stops at the end of the screen rather than
 * writing past it.
 */
#include "vga.h"
#include "string.h"

/* The text buffer. volatile because its only effect is on the display, which
 * the compiler cannot see: without it the writes may be discarded. */
static volatile uint16_t *const VGA_MEMORY = (volatile uint16_t *) 0xB8000;

static uint16_t vga_cell(char c, uint8_t colour)
{
    return (uint16_t) (uint8_t) c | (uint16_t) colour << 8;
}

void vga_clear(uint8_t colour)
{
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = vga_cell(' ', colour);
    }
}

/* Write text starting at the given cell. The write stops at the end of the
 * buffer rather than continuing into whatever follows it in memory. */
void vga_write(int row, int column, const char *text, uint8_t colour)
{
    const size_t length = strlen(text);
    for (size_t i = 0; i < length; i++) {
        const int at = row * VGA_WIDTH + column + (int) i;
        if (at < 0 || at >= VGA_WIDTH * VGA_HEIGHT) {
            return;
        }
        VGA_MEMORY[at] = vga_cell(text[i], colour);
    }
}

/* Write value as 0xXXXXXXXX. Eight digits always, because a number whose
 * width changes is harder to read in a column than one with leading zeros. */
void vga_write_hex(int row, int column, uint32_t value, uint8_t colour)
{
    static const char digits[] = "0123456789ABCDEF";
    char text[11];

    text[0] = '0';
    text[1] = 'x';
    for (int i = 0; i < 8; i++) {
        text[2 + i] = digits[(value >> (28 - i * 4)) & 0xF];
    }
    text[10] = '\0';

    vga_write(row, column, text, colour);
}
