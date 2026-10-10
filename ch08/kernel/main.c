/* spark -- kernel/main.c
 *
 * Measure polling against interrupts on the same device, then read a line
 * from the keyboard.
 */
#include "types.h"
#include "string.h"
#include "vga.h"
#include "printf.h"
#include "io.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "panic.h"
#include "timer.h"
#include "keyboard.h"

#define RULE_WIDTH 62
#define HZ         100

/* Typing speed used to turn a count of status reads into reads per
 * character. Five characters a second is roughly sixty words a minute. */
#define CHARS_PER_SECOND 5

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
    vga_putchar('\n');
}

/* Poll the controller for one second, as a kernel without interrupts would
 * have to, counting the status reads.
 *
 * IRQ 1 is still masked at this point, so no handler competes for the bytes
 * and every one that arrives is seen by this loop. */
static uint32_t poll_one_second(uint32_t *found)
{
    uint32_t asked = 0;
    uint32_t got = 0;
    uint32_t until = timer_ticks() + HZ;

    while (timer_ticks() < until) {
        asked++;
        if (inb(KBD_STATUS) & KBD_OUTPUT_FULL) {
            uint8_t code = inb(KBD_DATA);
            if (!(code & 0x80)) {
                got++;
            }
        }
    }

    *found = got;
    return asked;
}

/* The same second with interrupts enabled. Every hlt returns because an
 * interrupt arrived, so the count is the number of times the processor woke
 * at all. */
static uint32_t idle_one_second(void)
{
    uint32_t woke = 0;
    uint32_t until = timer_ticks() + HZ;

    while (timer_ticks() < until) {
        __asm__ volatile ("hlt");
        woke++;
    }

    return woke;
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    (void) magic;
    (void) boot_info;

    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 8: the keyboard\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));
    timer_init(HZ);
    __asm__ volatile ("sti");

    /* One second of polling. */
    kprintf("polling, for one second, with nobody typing:\n");
    uint32_t found = 0;
    uint32_t asked = poll_one_second(&found);
    kprintf("  asked the controller  %u times\n", asked);
    kprintf("  found something       %u times\n", found);
    kprintf("  a fast typist gives   %u questions per answer\n",
            asked / CHARS_PER_SECOND);
    rule();

    /* One second of interrupt-driven input. */
    kbd_init();
    kprintf("interrupt-driven, for one second, the same:\n");
    uint32_t woke = idle_one_second();
    kprintf("  the handler ran       %u times\n", kbd_scancodes());
    kprintf("  the processor woke    %u times, all of them ticks\n",
            woke);
    kprintf("  and it asked the controller nothing at all.\n");
    rule();

    kprintf("type something and press enter:\n");
    char line[80];
    size_t n = kbd_readline(line, sizeof line);
    kprintf("you typed %u character(s): %s\n", (unsigned) n, line);
    kprintf("scancodes seen so far: %u\n", kbd_scancodes());
    rule();

    kprintf("idling. every keystroke still arrives.\n");
    for (;;) {
        char c = kbd_getchar();
        kprintf("%c", c);
    }
}
