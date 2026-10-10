/* spark -- kernel/vga.c
 *
 * Text-mode output: character placement, line wrapping, scrolling, and the
 * hardware cursor that follows them.
 */
#include "vga.h"
#include "io.h"
#include "string.h"

/* The text buffer. volatile because its only effect is on the display, which
 * the compiler cannot see: without it the writes may be discarded. */
static volatile uint16_t *const VGA_MEMORY = (volatile uint16_t *) 0xB8000;

/* The screen controller's registers are in the port space, not in memory:
 * write a register number to the index port, then read or write its value at
 * the data port. */
#define CRTC_INDEX 0x3D4
#define CRTC_DATA  0x3D5
#define CURSOR_HIGH 0x0E
#define CURSOR_LOW  0x0F

static int row;
static int column;
static uint8_t colour;

static uint16_t vga_cell(char c, uint8_t with)
{
    return (uint16_t) (uint8_t) c | (uint16_t) with << 8;
}

/* Move the hardware cursor to the next write position, which the controller
 * takes as one cell offset from the top left, sent as two bytes because each
 * register holds one. */
static void vga_move_cursor(void)
{
    const uint16_t at = (uint16_t) (row * VGA_WIDTH + column);

    outb(CRTC_INDEX, CURSOR_LOW);
    outb(CRTC_DATA, (uint8_t) (at & 0xFF));
    outb(CRTC_INDEX, CURSOR_HIGH);
    outb(CRTC_DATA, (uint8_t) (at >> 8));
}

/* Erase the character before the cursor.
 *
 * The cursor stops at the start of a line rather than moving up to the
 * previous one: restoring the old column would mean recording where each line
 * ended, and a line editor should not delete past the start of its input
 * anyway.
 */
void vga_backspace(void)
{
    if (column == 0) {
        return;
    }

    column--;
    VGA_MEMORY[row * VGA_WIDTH + column] =
        vga_cell(' ', colour);
    vga_move_cursor();
}

/* Put one character where the cursor is, without moving it. */
static void vga_put_cell(char c)
{
    VGA_MEMORY[row * VGA_WIDTH + column] = vga_cell(c, colour);
}

static void vga_blank_row(int which)
{
    for (int i = 0; i < VGA_WIDTH; i++) {
        VGA_MEMORY[which * VGA_WIDTH + i] = vga_cell(' ', colour);
    }
}

/* Move every row up by one and blank the last. Source and destination
 * overlap, so the copy must be memmove rather than memcpy. */
static void vga_scroll(void)
{
    memmove((void *) VGA_MEMORY,
            (const void *) (VGA_MEMORY + VGA_WIDTH),
            (VGA_HEIGHT - 1) * VGA_WIDTH * sizeof(uint16_t));

    vga_blank_row(VGA_HEIGHT - 1);
    row = VGA_HEIGHT - 1;
}

static void vga_newline(void)
{
    column = 0;
    if (++row >= VGA_HEIGHT) {
        vga_scroll();
    }
}

void vga_init(void)
{
    colour = vga_colour(VGA_LIGHT_GREY, VGA_BLACK);
    vga_clear();
}

void vga_clear(void)
{
    for (int i = 0; i < VGA_HEIGHT; i++) {
        vga_blank_row(i);
    }
    row = 0;
    column = 0;
    vga_move_cursor();
}

void vga_set_colour(uint8_t with)
{
    colour = with;
}

void vga_putchar(char c)
{
    switch (c) {
    case '\n':
        vga_newline();
        break;
    case '\r':
        column = 0;
        break;
    case '\t':
        /* Advance to the next multiple of eight columns. */
        do {
            vga_putchar(' ');
        } while (column % 8 != 0);
        break;
    case '\b':
        if (column > 0) {
            column--;
            vga_put_cell(' ');
        }
        break;
    default:
        vga_put_cell(c);
        if (++column >= VGA_WIDTH) {
            vga_newline();
        }
        break;
    }
    vga_move_cursor();
}

void vga_write(const char *text)
{
    for (size_t i = 0; text[i] != '\0'; i++) {
        vga_putchar(text[i]);
    }
}
