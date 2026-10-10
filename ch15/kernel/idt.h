/* spark -- kernel/idt.h
 *
 * The interrupt descriptor table: one gate per vector, naming the code the
 * processor runs when that vector is raised.
 *
 * Exceptions, hardware interrupts and the int instruction all arrive through
 * it. Until the table is loaded the processor has nowhere to look, and any
 * exception at all escalates to a triple fault.
 */
#ifndef SPARK_IDT_H
#define SPARK_IDT_H

#include "types.h"

/* 256 vectors, one per value of the one-byte vector number. The processor
 * reserves 0 to 31; the remaining 224 are for the kernel to assign. */
#define IDT_ENTRIES 256

/* One gate, in the layout the processor reads: eight bytes, with the handler
 * address split between the two ends of the entry.
 *
 * The split is inherited. The 80286 gate was six bytes around a 16-bit
 * offset, and the 80386 put the upper sixteen bits of a 32-bit offset in the
 * two bytes the older format left unused. */
struct idt_entry {
    uint16_t offset_low;   /* handler address, bits 15..0  */
    uint16_t selector;     /* the segment to run it in     */
    uint8_t  zero;         /* unused, and must be zero     */
    uint8_t  type_attr;    /* present, ring, gate type     */
    uint16_t offset_high;  /* handler address, bits 31..16 */
} __attribute__((packed));

_Static_assert(sizeof(struct idt_entry) == 8, "eight bytes");

/* What lidt is handed, and what sidt reads back. The same six
 * bytes in the same order as the GDT's, because it is the same
 * kind of register. */
struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

_Static_assert(sizeof(struct idt_ptr) == 6, "six bytes");

/* The two gate types, in the low four bits of type_attr.
 *
 * Entering through an interrupt gate clears IF, so no further interrupt is
 * taken while the handler runs; a trap gate leaves IF as it was. spark uses
 * interrupt gates throughout: a handler that can be re-entered by the source
 * it is servicing will exhaust the stack. */
#define IDT_GATE_INTERRUPT 0x0E
#define IDT_GATE_TRAP      0x0F

#define IDT_PRESENT  (1 << 7)
#define IDT_DPL(n)   ((n) << 5)

/* Build the table and load it. */
void idt_init(void);

/* Fill in one gate directly. Device drivers do not use this: every vector
 * already points at a stub, and isr_install registers the C handler the stub
 * dispatches to. */
void idt_set_gate(uint8_t vector, uint32_t handler,
                  uint16_t selector, uint8_t type_attr);

/* The table the processor is actually using, read back from IDTR. */
void idt_current(struct idt_ptr *out);

#endif
