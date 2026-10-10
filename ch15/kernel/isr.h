/* spark -- kernel/isr.h
 *
 * The C side of an interrupt: the state the stubs in isr.asm have saved by
 * the time a handler runs, and the interface for claiming a vector.
 */
#ifndef SPARK_ISR_H
#define SPARK_ISR_H

#include "types.h"

/* Vectors for the hardware interrupts, once pic_init has remapped them clear
 * of the processor's own. IRQ 0 is the timer and IRQ 1 the keyboard. */
#define IRQ_BASE 32
#define IRQ(n)   (IRQ_BASE + (n))

/* The interrupted machine state, as it lies on the stack.
 *
 * The field order is fixed by the hardware and by isr.asm, which pushes from
 * the bottom of this list to the top; the layout in memory is the reverse of
 * the push order. Reordering a field makes every handler read a neighbouring
 * register, and the assertions below are what catch it.
 *
 * esp_at_pusha is the stack pointer pusha recorded, not the interrupted
 * code's, and has no legitimate use. The outer ss and esp are pushed only
 * when the interrupt crossed a privilege boundary; spark runs entirely in
 * ring 0, so they are omitted rather than declared and wrong. */
struct registers {
    uint32_t ds;

    uint32_t edi, esi, ebp, esp_at_pusha;
    uint32_t ebx, edx, ecx, eax;

    uint32_t vector;   /* the entry we came in by      */
    uint32_t error;    /* the processor's, or a zero   */

    uint32_t eip, cs, eflags;

    /* Pushed by the processor only when the interrupted code was running at
     * a lower privilege than the handler, which for spark means ring 3.
     * From an interrupt taken in ring 0 these two words are not part of the
     * frame at all and reading them reads whatever was above it. Guard
     * every use with isr_from_user. */
    uint32_t useresp, ss;
};

/* True if this frame came from user code. The processor does not record the
 * ring anywhere separate; it is the low two bits of the cs that was saved. */
static inline bool isr_from_user(const struct registers *regs)
{
    return (regs->cs & 3u) == 3u;
}

/* Sixteen words in the stub's push order, of which the last two are only
 * really there when the frame came from ring 3. Every field has the same
 * type, so swapping two of them compiles cleanly and misreads every frame
 * from then on; these assertions are the only check that the layout still
 * matches isr.asm. */
_Static_assert(sizeof(struct registers) == 64,
               "sixteen words");
_Static_assert(offsetof(struct registers, ds) == 0,
               "ds is where esp points");
_Static_assert(offsetof(struct registers, eax) == 32,
               "the last one pusha wrote");
_Static_assert(offsetof(struct registers, vector) == 36,
               "the stub pushed this");
_Static_assert(offsetof(struct registers, error) == 40,
               "and this, under it");
_Static_assert(offsetof(struct registers, eip) == 44,
               "the processor's own three");

/* A handler receives the frame and may modify it. Writing regs->eip changes
 * where the stub's iret returns to, which is how a fault handler resumes past
 * a faulting instruction instead of re-running it. */
typedef void (*isr_handler_t)(struct registers *regs);

/* Point one vector at a C function; NULL restores the default, which panics
 * for an exception and acknowledges and ignores a hardware interrupt. */
void isr_install(uint8_t vector, isr_handler_t handler);

/* Intel's mnemonic for a vector, such as "#DE", and its meaning in words.
 * Both are "" for a vector the processor does not reserve. */
const char *isr_mnemonic(uint32_t vector);
const char *isr_description(uint32_t vector);

/* True if the processor pushes an error code for this vector. The stubs push
 * a zero for the others so that every frame has one shape, so this is what
 * distinguishes a real error code from the placeholder. */
bool isr_has_error_code(uint32_t vector);

/* Print a frame, one line per group of registers. Used by the default
 * exception handler and by panic_with. */
void isr_dump(const struct registers *regs);

#endif
