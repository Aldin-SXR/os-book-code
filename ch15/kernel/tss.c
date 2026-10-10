/* spark -- kernel/tss.c
 *
 * One structure, two fields that matter, and the instruction that tells the
 * processor where to find it.
 */
#include "tss.h"
#include "gdt.h"
#include "string.h"

/* The layout is the processor's, and the order cannot be changed. spark
 * writes two of these fields and reads none of the rest. */
struct tss_entry {
    uint32_t prev;
    uint32_t esp0, ss0;    /* the ring-0 stack, and all we use */
    uint32_t esp1, ss1;
    uint32_t esp2, ss2;
    uint32_t cr3, eip, eflags;
    uint32_t eax, ecx, edx, ebx;
    uint32_t esp, ebp, esi, edi;
    uint32_t es, cs, ss, ds, fs, gs;
    uint32_t ldt;
    uint16_t trap, iomap_base;
} __attribute__((packed));

_Static_assert(sizeof(struct tss_entry) == 104, "the 386");

static struct tss_entry tss;

/* Defined in tss_flush.asm: ltr has no expression in C. */
extern void tss_flush(uint16_t selector);

void tss_init(void)
{
    memset(&tss, 0, sizeof tss);

    /* The stack segment is always the kernel's. Only esp0 changes, and it
     * changes on every context switch. */
    tss.ss0 = GDT_KERNEL_DATA;
    tss.esp0 = 0;

    /* Past the end of the segment, which is how the 386 says there is no
     * I/O permission bitmap. Leave it out and the processor reads the
     * bitmap from whatever follows this structure in memory. */
    tss.iomap_base = (uint16_t) sizeof tss;

    gdt_set_tss((uint32_t) (uintptr_t) &tss, sizeof tss);
    tss_flush(GDT_TSS);
}

void tss_set_stack(uint32_t esp0)
{
    tss.esp0 = esp0;
}

uint32_t tss_stack(void)
{
    return tss.esp0;
}
