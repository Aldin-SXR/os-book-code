/* spark -- kernel/main.c
 *
 * Bring up the subsystems in dependency order, then show what translation
 * buys: a heap whose addresses run in one line over frames that do not, two
 * names for one frame, and a page fault answered rather than fatal.
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
#include "vmm.h"
#include "heap.h"

#define RULE_WIDTH 62
#define HZ         100

/* A second name for a frame the heap already owns, chosen far from anything
 * the direct map or the heap uses. */
#define ALIAS 0xE0000000u

/* An address nothing maps until the fault handler maps it. */
#define ON_DEMAND 0xE8000000u

#define PATTERN 0xC0FFEE
#define SECOND  0xDECAFBAD

/* How many frames to hold back, so that the frames left for the heap to grow
 * into are not next to each other. */
#define SCATTER 64

static void *held[SCATTER];

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
}

/* The physical address behind a virtual one, or 0 when nothing maps it.
 * Frame 0 is never handed out, so 0 is free to mean "no translation". */
static uint32_t behind(uint32_t virt)
{
    uint32_t phys;

    return vmm_translate(virt, &phys) ? phys : 0;
}

/* Take every second free frame, so that what remains is a comb rather than a
 * run. The odd ones are kept and the even ones given straight back. */
static void scatter(void)
{
    for (uint32_t i = 0; i < SCATTER; i++) {
        held[i] = pmm_alloc_frame();
    }

    for (uint32_t i = 0; i < SCATTER; i += 2) {
        pmm_free_frame(held[i]);
        held[i] = NULL;
    }
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 11: virtual memory and paging\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));
    timer_init(HZ);
    kbd_init();
    __asm__ volatile ("sti");

    pmm_init(magic, boot_info);

    uint32_t before = pmm_free_frames();
    vmm_init();

    kprintf("\ndirect map    0x00000000 to 0x%08x, %u tables\n",
            pmm_top(), vmm_table_count());
    kprintf("paging on     cr3 0x%08x, and %u frames went on it\n",
            vmm_directory(), before - pmm_free_frames());
    rule();

    heap_init();

    /* One address, taken apart the way the processor takes it apart, and
     * then put through the tables it indexes. */
    uint32_t sample = HEAP_VIRTUAL_BASE + 0x1234u;

    kprintf("\nthe heap      virtual 0x%08x, physical 0x%08x\n",
            heap_base(), behind(heap_base()));
    kprintf("0x%08x    dir %u, table %u, offset 0x%03x\n",
            sample, PAGE_DIR_INDEX(sample),
            PAGE_TABLE_INDEX(sample), PAGE_OFFSET(sample));
    kprintf("              and the tables make it 0x%08x\n",
            behind(sample));
    kprintf("page tables   %u of a possible %u, %u KiB\n",
            vmm_table_count(), ENTRIES_PER_TABLE,
            vmm_table_count() * PAGE_SIZE / 1024u);
    rule();

    /* Break up physical memory, then make the heap grow into what is left.
     * The request is larger than the whole heap, so it cannot be answered
     * out of the block already there. */
    scatter();
    void *big = kmalloc(200000);

    kprintf("\nthe heap grew into a machine with holes in it:\n");
    kprintf("  virtual   ");
    for (uint32_t i = 0; i < 4; i++) {
        kprintf(" 0x%08x", heap_base() + 0x10000u + i * PAGE_SIZE);
    }
    kprintf("\n  physical  ");
    for (uint32_t i = 0; i < 4; i++) {
        kprintf(" 0x%08x",
                behind(heap_base() + 0x10000u + i * PAGE_SIZE));
    }
    kprintf("\n");

    for (uint32_t i = 1; i < SCATTER; i += 2) {
        pmm_free_frame(held[i]);
    }
    kfree(big);
    rule();

    /* One frame, two names. Neither address is the frame's. */
    uint32_t frame = behind(heap_base());

    /* volatile so that the write and the read below really happen: the
     * point is to make the processor use the tables, and a compiler that
     * folded the two together would prove nothing. */
    vmm_map(ALIAS, frame, PAGE_PRESENT | PAGE_WRITE);
    *(volatile uint32_t *) heap_base() = PATTERN;

    kprintf("\ntwo names     0x%08x and 0x%08x\n", heap_base(), ALIAS);
    kprintf("one frame     0x%08x, holding 0x%06x from both\n",
            frame, *(volatile uint32_t *) ALIAS);
    rule();

    /* A fault that is answered instead of fatal. */
    vmm_demand_region(ON_DEMAND, PAGE_SIZE);
    kprintf("\n0x%08x maps nothing, and we write to it anyway\n",
            ON_DEMAND);

    *(volatile uint32_t *) ON_DEMAND = SECOND;

    kprintf("  fault %u at 0x%08x, then a frame at 0x%08x\n",
            vmm_fault_count(), vmm_last_fault(), behind(ON_DEMAND));
    kprintf("  the write went through: 0x%08x\n",
            *(volatile uint32_t *) ON_DEMAND);
    rule();

    /* Point the same name at the other frame. The processor cached the old
     * translation, so this only works because vmm_map ends in invlpg. */
    vmm_map(ALIAS, behind(ON_DEMAND), PAGE_PRESENT | PAGE_WRITE);

    kprintf("\n0x%08x now points at 0x%08x instead, and reads\n",
            ALIAS, behind(ALIAS));
    kprintf("  0x%08x, which is what the other frame holds\n",
            *(volatile uint32_t *) ALIAS);
    rule();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
