/* spark -- kernel/main.c
 *
 * Bring up the subsystems in dependency order, then exercise the heap: what
 * one byte costs, what happens to a hole, what a full heap looks like when
 * every second block is given back, and what the allocator does when it runs
 * out of room.
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
#include "heap.h"

#define RULE_WIDTH 62
#define HZ         100

/* The size the fragmentation test allocates. Small enough that the header is
 * a visible fraction of it, large enough to be a plausible kernel object. */
#define SMALL 96

/* Enough pointers for every SMALL block a 16-frame heap can hold. */
#define MAX_BLOCKS 1024

/* Allocate and free one block this many times, to show that a heap which
 * reuses memory does not grow and a bump allocator would. */
#define CHURN 1000

static void *blocks[MAX_BLOCKS];

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 10: the kernel heap\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));
    timer_init(HZ);
    kbd_init();
    __asm__ volatile ("sti");

    pmm_init(magic, boot_info);

    uint32_t pmm_before = pmm_free_frames();
    heap_init();

    struct heap_stats s;
    heap_get_stats(&s);

    kprintf("\nthe heap      0x%08x, %u frames of the pmm's %u\n",
            heap_base(), s.frames, pmm_before);
    kprintf("one header    %u bytes, a payload rounds up to %u, %u free\n",
            heap_header_size(), HEAP_ALIGN, s.free);
    rule();

    /* Three blocks in a row, so that the addresses show the header sitting
     * between them. */
    void *a = kmalloc(1);
    void *b = kmalloc(64);
    void *c = kmalloc(40);

    kprintf("\na kmalloc(1)   %p, holding %u bytes and costing %u\n",
            a, heap_block_size(a),
            heap_block_size(a) + heap_header_size());
    kprintf("b kmalloc(64)  %p\n", b);
    kprintf("c kmalloc(40)  %p\n", c);

    uint32_t hole = heap_block_size(b);
    kfree(b);
    heap_get_stats(&s);
    kprintf("kfree(b)       a hole of %u between a and c, %u free blocks\n",
            hole, s.free_blocks);

    /* Small enough for the hole, and with enough left over to split. */
    void *d = kmalloc(24);
    heap_get_stats(&s);
    kprintf("d kmalloc(24)  %p, in that hole, which splits: %u again\n",
            d, s.free_blocks);

    /* Too big for what is left of the hole, so it goes above c. */
    void *e = kmalloc(64);
    kprintf("e kmalloc(64)  %p, past c: %u was not enough\n",
            e, hole - heap_block_size(d) - heap_header_size());

    kfree(a);
    kfree(c);
    kfree(d);
    kfree(e);
    heap_get_stats(&s);
    kprintf("freed all five, and %u block(s) are left, of %u bytes\n",
            s.blocks, s.free);
    rule();

    /* Fill the heap exactly. Each block costs a header plus its payload, so
     * the number that fits is a division, and the last one is the remainder
     * folded into the block before it. */
    uint32_t each = heap_header_size() + SMALL;
    uint32_t n = s.free / each;

    if (n > MAX_BLOCKS) {
        panic("main: the heap holds more blocks than there are pointers");
    }

    for (uint32_t i = 0; i < n; i++) {
        blocks[i] = kmalloc(SMALL);

        if (blocks[i] == NULL) {
            panic("main: the heap ran out before the arithmetic said it would");
        }
    }

    heap_get_stats(&s);
    kprintf("\n%u x kmalloc(%u) fills it: %u free, %u in headers\n",
            n, SMALL, s.free, s.overhead);
    kprintf("the last one got %u: %u spare, and a split needs %u\n",
            s.used - (n - 1) * SMALL,
            s.used - (n - 1) * SMALL - SMALL,
            heap_header_size() + HEAP_ALIGN);

    for (uint32_t i = 1; i < n; i += 2) {
        kfree(blocks[i]);
        blocks[i] = NULL;
    }

    heap_get_stats(&s);
    kprintf("freeing every second block leaves %u holes:\n", s.free_blocks);
    kprintf("  free        %u bytes, and the largest hole is %u\n",
            s.free, s.largest_free);

    uint32_t frames_before = s.frames;
    uint32_t pmm_at_grow = pmm_free_frames();
    void *big = kmalloc(SMALL + 1);
    heap_get_stats(&s);

    kprintf("  so kmalloc(%u) cannot be served from them, and is not:\n",
            SMALL + 1);
    kprintf("  the heap    %u -> %u frames, and the pmm %u -> %u\n",
            frames_before, s.frames, pmm_at_grow, pmm_free_frames());

    kfree(big);

    for (uint32_t i = 0; i < n; i += 2) {
        kfree(blocks[i]);
        blocks[i] = NULL;
    }

    heap_get_stats(&s);
    kprintf("freed the lot: back to %u block(s), of %u bytes\n",
            s.blocks, s.free);
    rule();

    /* Allocate and free the same size over and over. A heap that reuses
     * memory stays the size it is; one that only ever moves a pointer
     * forward would need every byte of it again. */
    for (uint32_t i = 0; i < CHURN; i++) {
        kfree(kmalloc(SMALL));
    }

    heap_get_stats(&s);
    kprintf("\n%u x kmalloc(%u) and kfree: still %u frames\n",
            CHURN, SMALL, s.frames);
    kprintf("a bump allocator would have wanted %u bytes, %u frames\n",
            CHURN * each,
            (CHURN * each + FRAME_SIZE - 1) / FRAME_SIZE);
    rule();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
