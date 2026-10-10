/* spark -- kernel/main.c
 *
 * Start the timer, report the rate the chip actually delivers, and show what
 * a periodic interrupt buys: sleeping without spinning, and preemption of a
 * loop that yields nothing.
 */
#include "types.h"
#include "string.h"
#include "vga.h"
#include "printf.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "panic.h"
#include "timer.h"

#define RULE_WIDTH 62
#define WANTED_HZ  100

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
    vga_putchar('\n');
}

/* Print a rate given in millihertz as a decimal. The kernel has no floating
 * point and needs none: the whole part and the remainder are printed
 * separately. */
static void print_rate(uint32_t millihertz)
{
    kprintf("%u.%03u Hz", millihertz / 1000u, millihertz % 1000u);
}

/* A loop that yields nothing: it calls no function and waits on nothing.
 * The tick count taken across it shows how often the processor was taken
 * away from it regardless. */
static uint32_t busy_loop(void)
{
    uint32_t before = timer_ticks();

    for (volatile uint32_t i = 0; i < 40000000u; i++) {
    }

    return timer_ticks() - before;
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    (void) magic;
    (void) boot_info;

    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 7: the timer\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));

    uint32_t got = timer_init(WANTED_HZ);

    kprintf("  asked for     %u Hz\n", WANTED_HZ);
    kprintf("  divisor       %u  (of %u Hz)\n",
            (unsigned) timer_divisor(), PIT_FREQUENCY);
    kprintf("  really got    ");
    print_rate(got);
    kprintf("\n  irq 0         open, mask now 0x%04X\n", pic_masks());
    rule();

    /* Nothing has arrived yet, because the flag has been clear
     * since the boot loader. This is the line that starts the
     * clock. */
    __asm__ volatile ("sti");

    kprintf("waiting for the first tick...\n");
    while (timer_ticks() == 0) {
        __asm__ volatile ("hlt");
    }
    kprintf("  it arrived, and the processor was asleep until it did.\n");
    rule();

    /* Measured from here rather than from boot, because the
     * first tick lands wherever it lands and the interesting
     * number is the interval rather than the offset. */
    uint32_t start = timer_uptime_ms();

    kprintf("three waits of half a second:\n");
    for (int i = 1; i <= 3; i++) {
        timer_sleep(500);
        kprintf("  %d  after %u ms  (tick %u)\n",
                i, timer_uptime_ms() - start, timer_ticks());
    }
    rule();

    kprintf("a loop that never yields:\n");
    uint32_t during = busy_loop();
    kprintf("  it called nothing, and was interrupted %u times.\n",
            during);
    kprintf("  that is how a kernel takes the processor back.\n");
    rule();

    kprintf("idling. uptime climbs while nothing runs.\n");
    for (;;) {
        __asm__ volatile ("hlt");
    }
}
