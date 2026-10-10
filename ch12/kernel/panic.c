/* spark -- kernel/panic.c
 *
 * Report an unrecoverable fault and halt the machine.
 */
#include "panic.h"
#include "vga.h"
#include "printf.h"

/* Halt with interrupts disabled, so nothing else runs after the report.
 * The loop is required: hlt returns when a non-maskable interrupt arrives,
 * and there is nothing to do at that point except halt again. */
static void halt_forever(void) __attribute__((noreturn));

static void halt_forever(void)
{
    for (;;) {
        __asm__ volatile ("cli; hlt");
    }
}

void panic(const char *message)
{
    vga_set_colour(vga_colour(VGA_WHITE, VGA_RED));
    kprintf("\nPANIC: %s\n", message);
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));

    halt_forever();
}

void panic_with(const char *message,
                const struct registers *regs)
{
    vga_set_colour(vga_colour(VGA_WHITE, VGA_RED));
    kprintf("\nPANIC: %s\n", message);
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));

    isr_dump(regs);

    halt_forever();
}
