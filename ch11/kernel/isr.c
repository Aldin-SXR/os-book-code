/* spark -- kernel/isr.c
 *
 * Vector dispatch: the single C entry point the assembly stubs call, the
 * table of registered handlers, and the names the processor's own vectors
 * are reported by.
 */
#include "isr.h"
#include "idt.h"
#include "pic.h"
#include "panic.h"
#include "printf.h"

/* The processor's thirty-two vectors, under Intel's names. The blank entries
 * are reserved and never raised by hardware, so one arriving points at a
 * fault in the table rather than in the interrupted code. */
static const struct {
    const char *mnemonic;
    const char *description;
} exceptions[32] = {
    { "#DE", "divide error" },
    { "#DB", "debug" },
    { "",    "non-maskable interrupt" },
    { "#BP", "breakpoint" },
    { "#OF", "overflow" },
    { "#BR", "bound range exceeded" },
    { "#UD", "invalid opcode" },
    { "#NM", "device not available" },
    { "#DF", "double fault" },
    { "",    "coprocessor segment overrun" },
    { "#TS", "invalid TSS" },
    { "#NP", "segment not present" },
    { "#SS", "stack-segment fault" },
    { "#GP", "general protection" },
    { "#PF", "page fault" },
    { "",    "reserved" },
    { "#MF", "x87 floating-point error" },
    { "#AC", "alignment check" },
    { "#MC", "machine check" },
    { "#XM", "SIMD floating-point error" },
    { "#VE", "virtualisation" },
    { "#CP", "control protection" },
    { "",    "reserved" }, { "", "reserved" }, { "", "reserved" },
    { "",    "reserved" }, { "", "reserved" }, { "", "reserved" },
    { "#HV", "hypervisor injection" },
    { "#VC", "VMM communication" },
    { "#SX", "security" },
    { "",    "reserved" },
};

/* The seven vectors the processor pushes an error code for. For every other
 * vector the value in that slot is the zero isr.asm pushed. */
bool isr_has_error_code(uint32_t vector)
{
    switch (vector) {
    case 8: case 10: case 11: case 12:
    case 13: case 14: case 17:
        return true;
    default:
        return false;
    }
}

const char *isr_mnemonic(uint32_t vector)
{
    return vector < 32 ? exceptions[vector].mnemonic : "";
}

const char *isr_description(uint32_t vector)
{
    if (vector < 32) {
        return exceptions[vector].description;
    }
    if (vector >= IRQ_BASE && vector < IRQ_BASE + 16) {
        return "hardware interrupt";
    }
    return "no such vector";
}

static isr_handler_t handlers[IDT_ENTRIES];

void isr_install(uint8_t vector, isr_handler_t handler)
{
    handlers[vector] = handler;
}

void isr_dump(const struct registers *regs)
{
    const char *mnemonic = isr_mnemonic(regs->vector);

    kprintf("  vector %u", (unsigned) regs->vector);
    if (mnemonic[0] != '\0') {
        kprintf("  %s", mnemonic);
    }
    kprintf("  %s\n", isr_description(regs->vector));

    kprintf("  error 0x%08X eip 0x%08X cs 0x%04X eflags 0x%08X\n",
            regs->error, regs->eip,
            (unsigned) regs->cs, regs->eflags);
    kprintf("  eax 0x%08X ebx 0x%08X ecx 0x%08X edx 0x%08X\n",
            regs->eax, regs->ebx, regs->ecx, regs->edx);
    kprintf("  esi 0x%08X edi 0x%08X ebp 0x%08X ds 0x%04X\n",
            regs->esi, regs->edi, regs->ebp,
            (unsigned) regs->ds);
}

/* The common tail of all forty-eight stubs. Declared before the definition
 * because nothing in C calls it: every call site is in isr.asm. */
void isr_dispatch(struct registers *regs);

void isr_dispatch(struct registers *regs)
{
    isr_handler_t handler = handlers[regs->vector];

    if (handler) {
        handler(regs);
    } else if (regs->vector < 32) {
        /* An unclaimed processor exception is a fault in the kernel
         * itself. There is nothing to report it to and no state worth
         * returning to. */
        panic_with("unhandled exception", regs);
    }

    /* The controller raises nothing further on a line until that line is
     * acknowledged. It happens here rather than in the handler so that an
     * unclaimed or misbehaving interrupt cannot silence the chip. */
    uint32_t irq = regs->vector - IRQ_BASE;

    if (regs->vector >= IRQ_BASE && irq < 16) {
        pic_eoi((uint8_t) irq);
    }
}
