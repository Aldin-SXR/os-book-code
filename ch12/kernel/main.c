/* spark -- kernel/main.c
 *
 * Bring up the subsystems in dependency order, then run three processes on
 * one processor: two that take turns and print as they go, and one that
 * yields a hundred thousand times so that a switch can be timed.
 */
#include "types.h"
#include "vga.h"
#include "printf.h"
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
#include "proc.h"

#define RULE_WIDTH 62
#define HZ         100

/* How many times each of the two takes a turn. */
#define TURNS 3

/* Yields in the timing run. Each one is a switch away and another back, so
 * the number of switches is twice this. */
#define CHURN 1000000u

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
}

/* The body both of the first two processes run. The count is a local, so
 * each process has its own on its own stack, and neither can see the
 * other's. That is the whole of what a separate stack buys. */
static void take_turns(void)
{
    uint32_t count;

    /* Taking the address forces the compiler to keep it in memory rather
     * than in a register, which is what makes the address printable. The
     * two processes run this same line and print two different answers. */
    kprintf("  %s keeps its count at %p\n",
            proc_current()->name, (void *) &count);
    proc_yield();

    for (count = 1; count <= TURNS; count++) {
        kprintf("  %s %u", proc_current()->name, count);
        proc_yield();
    }

    kprintf("\n");
}

static void alpha(void) { take_turns(); }
static void beta(void)  { take_turns(); }

/* Nothing but switching, so that the time is the switch and not the work. */
static void churn(void)
{
    for (uint32_t i = 0; i < CHURN; i++) {
        proc_yield();
    }
}

/* kprintf pads numbers and not strings, so a column of names is padded here.
 * Adding it to Chapter 4's printf would be the other answer and a larger
 * change than this table is worth. */
static void pad(const char *s, uint32_t width)
{
    uint32_t n = 0;

    while (s[n] != '\0') {
        vga_putchar(s[n++]);
    }

    while (n++ < width) {
        vga_putchar(' ');
    }
}

static void print_table(void)
{
    kprintf("  pid  name   state    esp         stack\n");

    struct process *p = proc_first();

    do {
        kprintf("  %3u  ", p->pid);
        pad(p->name, 7);
        pad(proc_state_name(p->state), 9);
        kprintf("0x%08x  ", p->esp);

        if (p->stack == NULL) {
            kprintf("from boot.asm\n");
        } else {
            kprintf("0x%08x\n", (uint32_t) p->stack);
        }

        p = p->next;
    } while (p != proc_first());
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 12: processes and context switching\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));
    timer_init(HZ);
    kbd_init();
    __asm__ volatile ("sti");

    pmm_init(magic, boot_info);
    vmm_init();
    heap_init();
    proc_init();

    kprintf("\none process        %u bytes, and a stack of %u\n",
            (uint32_t) sizeof(struct process), PROC_STACK_BYTES);

    struct process *a = proc_create("alpha", alpha);
    struct process *b = proc_create("beta", beta);

    if (a == NULL || b == NULL) {
        panic("kmain: the heap could not hold two processes");
    }

    rule();

    kprintf("\n");
    print_table();
    rule();

    /* Yield until both of them have run out of turns. Each time round, the
     * next READY process gets the processor and gives it back. */
    kprintf("\ntaking turns:\n");

    while (a->state != PROC_DONE || b->state != PROC_DONE) {
        proc_yield();
    }

    kprintf("each count is a local on its own stack\n");
    rule();

    kprintf("\n");
    print_table();
    rule();

    /* One more process, doing nothing but handing the processor back, so
     * that the time is the switch. */
    struct process *c = proc_create("churn", churn);

    if (c == NULL) {
        panic("kmain: the heap could not hold a third process");
    }

    uint32_t before = timer_ticks();

    while (c->state != PROC_DONE) {
        proc_yield();
    }

    uint32_t ms = (timer_ticks() - before) * (1000u / HZ);

    kprintf("\n%u switches in %u ms, which is %u ns each\n",
            2u * CHURN, ms, ms * 1000000u / (2u * CHURN));
    kprintf("%u switches in all, and %u processes made them\n",
            proc_switches(), proc_count());
    rule();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
