/* spark -- kernel/panic.h
 *
 * Unrecoverable failure: report the state and halt.
 *
 * A kernel has nothing above it to return an error to. Continuing past an
 * impossible condition produces a second failure somewhere unrelated, which
 * carries no information about the first, so the fault is reported where it
 * is detected and the machine is stopped there.
 */
#ifndef SPARK_PANIC_H
#define SPARK_PANIC_H

#include "types.h"
#include "isr.h"

/* Print the message and halt. Never returns. */
void panic(const char *message) __attribute__((noreturn));

/* The same, with the interrupted machine state printed under it. This is
 * where an unhandled exception ends. */
void panic_with(const char *message,
                const struct registers *regs)
    __attribute__((noreturn));

#endif
