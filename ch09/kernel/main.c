/* spark -- kernel/main.c
 *
 * Bring up the subsystems in dependency order, then exercise the frame
 * allocator and report what the machine actually holds.
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
#include "pmm.h"

#define RULE_WIDTH 62
#define HZ         100

/* One pass over the whole of memory takes less than a tick, so the
 * measurement below runs this many passes and the reader divides back. */
#define ROUNDS 20

/* Test pattern written into an allocated frame. The value is arbitrary and
 * chosen to be recognisable in a hex dump. */
#define PATTERN 0xC0FFEE

/* A horizontal rule, deliberately without a trailing newline. The screen
 * scrolls when a newline is written on the bottom row rather than when the
 * row fills, so every block below opens with its own newline instead and the
 * output ends without scrolling the map off the top. */
static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
}

static uint32_t kib(uint32_t frames)
{
    return frames * (FRAME_SIZE / 1024u);
}

/* Allocate frames until the allocator refuses, counting them and keeping the
 * highest address it returned. The addresses themselves are not recorded:
 * storing tens of thousands of them would need more memory than the test is
 * measuring, and pmm_init rebuilds the bitmap from the map afterwards. */
static uint32_t take_everything(void **last)
{
    uint32_t taken = 0;
    void *frame;

    while ((frame = pmm_alloc_frame()) != NULL) {
        *last = frame;
        taken++;
    }

    return taken;
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 9: physical memory\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));
    timer_init(HZ);
    kbd_init();
    __asm__ volatile ("sti");

    /* The loader's magic number and information structure, passed through
     * from boot.asm. */
    pmm_init(magic, boot_info);

    kprintf("\nthe machine, as it describes itself:\n");
    pmm_dump_map();
    rule();

    uint32_t kernel_frames =
        (pmm_kernel_end() - pmm_kernel_start()) / FRAME_SIZE;

    kprintf("\nusable        %u frames, %u KiB (%s)\n",
            pmm_total_frames(), kib(pmm_total_frames()),
            pmm_map_was_measured() ? "measured" : "ASSUMED");
    kprintf("the kernel    0x%08x to 0x%08x, %u frames\n",
            pmm_kernel_start(), pmm_kernel_end(), kernel_frames);
    kprintf("the bitmap    %u KiB of those, covering all 4 GiB\n",
            (uint32_t) (PMM_MAX_FRAMES / 8u / 1024u));
    kprintf("free now      %u frames, %u KiB\n",
            pmm_free_frames(), kib(pmm_free_frames()));
    rule();

    void *a = pmm_alloc_frame();
    void *b = pmm_alloc_frame();
    void *c = pmm_alloc_frame();
    kprintf("\nalloc         %p, %p, %p\n", a, b, c);

    /* Confirm the frame is addressable memory rather than a number. */
    *(volatile uint32_t *) b = PATTERN;
    kprintf("wrote 0x%06x into %p, and got 0x%06x back\n",
            PATTERN, b, *(volatile uint32_t *) b);

    pmm_free_frame(b);
    kprintf("freed that one, asked again, got %p\n",
            pmm_alloc_frame());

    pmm_free_frame(a);
    pmm_free_frame(b);
    pmm_free_frame(c);
    rule();

    kprintf("\nemptying the machine, a frame at a time:\n");

    void *last = NULL;
    uint32_t taken = 0;
    uint32_t before = timer_ticks();

    for (uint32_t i = 0; i < ROUNDS; i++) {
        taken = take_everything(&last);
        pmm_init(magic, boot_info);
    }

    uint32_t ms = (timer_ticks() - before) * (1000u / HZ);

    /* The highest frame is the one an assumed memory size would get wrong,
     * so it is the one worth touching. */
    *(volatile uint32_t *) last = PATTERN;

    kprintf("  handed out  %u frames, and NULL next\n", taken);
    kprintf("  the top     %p, holding 0x%06x\n",
            last, *(volatile uint32_t *) last);
    kprintf("  %u rounds   %u ms, and %u frames free again\n",
            ROUNDS, ms, pmm_free_frames());
    rule();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
