/* spark -- kernel/main.c
 *
 * Install the interrupt machinery, then raise two exceptions on purpose: one
 * the kernel recovers from and one it does not.
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

#define RULE_WIDTH 62

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
    vga_putchar('\n');
}

/* A handler for the breakpoint vector, which the code below raises
 * deliberately.
 *
 * int3 is a trap rather than a fault: the saved eip is the address after the
 * instruction, so returning carries on past it. Returning from a fault
 * handler would re-run the instruction that faulted.
 */
static void on_breakpoint(struct registers *regs)
{
    kprintf("  caught it: %s at eip 0x%08X\n",
            isr_description(regs->vector), regs->eip);
}

/* Raise a divide error.
 *
 * Written in assembly because the C equivalent does not survive optimisation,
 * volatile included: division by zero is undefined, so the compiler may
 * assume it never happens and delete the division while keeping the volatile
 * reads. Measured with clang for this target: the idiv is emitted at -O0 and
 * absent from -O1 upwards, and spark is built at -O2.
 *
 * divl divides edx:eax by its operand, so edx is cleared and eax set to 1.
 * The zero arrives in ecx as a variable rather than a literal, which the
 * assembler would reject. */
static void divide_by_zero(void)
{
    uint32_t zero = 0;

    __asm__ volatile ("xorl %%edx, %%edx\n\t"
                      "movl $1, %%eax\n\t"
                      "divl %%ecx"
                      : : "c"(zero) : "eax", "edx", "cc");
}

static void show_tables(void)
{
    struct gdt_ptr gdtr;
    struct idt_ptr idtr;

    gdt_current(&gdtr);
    idt_current(&idtr);

    kprintf("  gdtr  base %p  limit %04X  (%u entries)\n",
            (void *) gdtr.base, gdtr.limit,
            (unsigned) ((gdtr.limit + 1u) / 8u));
    kprintf("  idtr  base %p  limit %04X  (%u entries)\n",
            (void *) idtr.base, idtr.limit,
            (unsigned) ((idtr.limit + 1u) / 8u));
    kprintf("  pic   lines 0..15 now at vectors %u..%u, mask 0x%04X\n",
            IRQ(0), IRQ(15), pic_masks());
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    (void) magic;
    (void) boot_info;

    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 6: interrupts and exceptions\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));

    show_tables();
    rule();

    /* The processor will not deliver a hardware interrupt while
     * IF is clear, and it has been clear since the boot loader.
     * Every line is masked, so nothing arrives yet; what this
     * proves is that the table is good enough to be trusted
     * with the flag. */
    __asm__ volatile ("sti");
    kprintf("interrupts enabled, and we are still here.\n\n");

    /* The recoverable one: a claimed vector whose handler returns. */
    isr_install(3, on_breakpoint);
    kprintf("raising a breakpoint on purpose:\n");
    __asm__ volatile ("int3");
    kprintf("  and we carried on past it.\n");
    rule();

    /* The unrecoverable one: no handler claims vector 0, so isr_dispatch
     * panics and the frame is reported. Without an IDT the same division
     * triple-faults and resets the machine, leaving nothing to read.
     */
    kprintf("dividing by zero on purpose:\n");
    divide_by_zero();

    panic("unreachable: the divide should have trapped");
}
