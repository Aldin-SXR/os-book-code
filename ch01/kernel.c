/* spark -- kernel.c
 *
 * The whole of the kernel at this stage: clear the screen and write one line
 * to it.
 *
 * Nothing runs underneath this code. There is no printf, no malloc and no
 * file to write to; there is memory, and one region of it is wired to the
 * display.
 */
#include <stdint.h>

/* The VGA text buffer: 80 columns by 25 rows, two bytes per cell, with the
 * character in the low byte and its colour in the high byte. A write to this
 * memory is a character on the screen, with no driver in between. */
#define VGA_MEMORY ((volatile uint16_t *) 0xB8000)
#define VGA_WIDTH  80
#define VGA_HEIGHT 25

/* Attribute byte: foreground in the low nibble, background in the high one.
 * 0x0F is white on black. */
#define COLOUR 0x0F

void kmain(void)
{
    /* Clear the screen, which still holds whatever the firmware left. */
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        VGA_MEMORY[i] = (uint16_t) ' ' |
                        (uint16_t) COLOUR << 8;
    }

    const char *message = "spark is awake.";
    for (int i = 0; message[i] != '\0'; i++) {
        VGA_MEMORY[i] = (uint16_t) message[i]
                      | (uint16_t) COLOUR << 8;
    }
}
