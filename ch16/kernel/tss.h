/* spark -- kernel/tss.h
 *
 * The task state segment: one structure whose only job here is to say where
 * the kernel's stack is.
 *
 * The 80386 designed this for hardware task switching, which nobody uses and
 * which Chapter 12 replaced with four pushes and a pop. Two fields of it
 * survived, and they are not optional. When the processor takes an interrupt
 * while running in ring 3 it cannot keep using the user's stack, because the
 * user chose where that stack is and may have pointed it anywhere. So it
 * reads a fresh ss and esp out of the task state segment, switches to them,
 * and pushes the old pair on the new stack.
 *
 * That is the whole of what spark uses it for. Everything else in the 104
 * bytes stays zero.
 */
#ifndef SPARK_TSS_H
#define SPARK_TSS_H

#include "types.h"

/* Build the segment, put its descriptor in the table, and load it. Must run
 * after gdt_init, because it writes an entry of the table gdt_init built. */
void tss_init(void);

/* Say which stack the processor should switch to on the way in from ring 3.
 * Called once before the first entry to user mode, and again on every switch
 * from Chapter 12's scheduler, because each process has its own. */
void tss_set_stack(uint32_t esp0);

/* What the processor would use right now, read back from the structure. */
uint32_t tss_stack(void);

#endif
