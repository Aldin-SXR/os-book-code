/* spark -- kernel/idt.c
 *
 * Building the interrupt descriptor table and pointing every implemented
 * vector at its stub.
 */
#include "idt.h"
#include "gdt.h"
#include "syscall.h"
#include "string.h"

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr   idt_ptr;

/* Defined in isr.asm. These are entry points rather than functions: they end
 * in iret and must never be called from C. Only their addresses are used. */
extern void isr0(void), isr1(void), isr2(void);
extern void isr3(void), isr4(void), isr5(void);
extern void isr6(void), isr7(void), isr8(void);
extern void isr9(void), isr10(void), isr11(void);
extern void isr12(void), isr13(void), isr14(void);
extern void isr15(void), isr16(void), isr17(void);
extern void isr18(void), isr19(void), isr20(void);
extern void isr21(void), isr22(void), isr23(void);
extern void isr24(void), isr25(void), isr26(void);
extern void isr27(void), isr28(void), isr29(void);
extern void isr30(void), isr31(void);

extern void irq0(void), irq1(void), irq2(void);
extern void irq3(void), irq4(void), irq5(void);
extern void irq6(void), irq7(void), irq8(void);
extern void irq9(void), irq10(void), irq11(void);
extern void irq12(void), irq13(void), irq14(void);
extern void irq15(void);

/* The stubs in vector order, so that idt_init is a loop rather than
 * forty-eight near-identical lines that would drift apart. */
extern void isr128(void);

static void (*const stubs[])(void) = {
    isr0,  isr1,  isr2,  isr3,  isr4,  isr5,  isr6,  isr7,
    isr8,  isr9,  isr10, isr11, isr12, isr13, isr14, isr15,
    isr16, isr17, isr18, isr19, isr20, isr21, isr22, isr23,
    isr24, isr25, isr26, isr27, isr28, isr29, isr30, isr31,
    irq0,  irq1,  irq2,  irq3,  irq4,  irq5,  irq6,  irq7,
    irq8,  irq9,  irq10, irq11, irq12, irq13, irq14, irq15,
};

#define STUB_COUNT (sizeof stubs / sizeof stubs[0])

_Static_assert(STUB_COUNT == 48, "32 exceptions and 16 lines");

/* The handler address is stored in halves at opposite ends of the entry. */
void idt_set_gate(uint8_t vector, uint32_t handler,
                  uint16_t selector, uint8_t type_attr)
{
    idt[vector].offset_low  = (uint16_t) (handler & 0xFFFF);
    idt[vector].selector    = selector;
    idt[vector].zero        = 0;
    idt[vector].type_attr   = type_attr;
    idt[vector].offset_high = (uint16_t) (handler >> 16);
}

void idt_init(void)
{
    /* Every entry starts with its present bit clear, and the 208 vectors
     * with no stub stay that way. An attempt to use one raises a segment
     * not present fault, which does have a handler, instead of transferring
     * control to address zero. */
    memset(idt, 0, sizeof idt);

    const uint8_t attr = IDT_PRESENT | IDT_DPL(0) |
                         IDT_GATE_INTERRUPT;

    for (unsigned v = 0; v < STUB_COUNT; v++) {
        idt_set_gate((uint8_t) v,
                     (uint32_t) (uintptr_t) stubs[v],
                     GDT_KERNEL_CODE, attr);
    }

    /* The system call gate, and the two things about it that make ring 3
     * able to use it and unable to use anything else.
     *
     * Its DPL is 3. The DPL of a gate is the lowest privilege that may reach
     * it with an int instruction, so this one entry of the 256 is open to
     * user code and the other 255 are not. That is the whole of the
     * permission check: there is no list of allowed vectors anywhere.
     *
     * It is a trap gate rather than an interrupt gate, so the interrupt flag
     * is left as it was instead of being cleared. A system call can take a
     * while, and the timer of Chapter 7 should still be able to preempt a
     * process that is part-way through one. */
    idt_set_gate(SYSCALL_VECTOR,
                 (uint32_t) (uintptr_t) isr128,
                 GDT_KERNEL_CODE,
                 IDT_PRESENT | IDT_DPL(3) | IDT_GATE_TRAP);

    idt_ptr.limit = (uint16_t) (sizeof idt - 1);
    idt_ptr.base  = (uint32_t) (uintptr_t) idt;

    /* lidt takes the same six-byte operand as lgdt and needs no far jump
     * afterwards: no part of this table is cached, and each entry is read
     * when its vector is raised. */
    __asm__ volatile ("lidt %0" : : "m"(idt_ptr));
}

void idt_current(struct idt_ptr *out)
{
    __asm__ volatile ("sidt %0" : "=m"(*out));
}
