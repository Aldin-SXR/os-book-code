/* spark -- kernel/printf.h
 *
 * Formatted output to the screen.
 */
#ifndef SPARK_PRINTF_H
#define SPARK_PRINTF_H

#include "types.h"

/* Conversions: %d, %u, %x, %X, %c, %s, %p and %%, each of the numeric ones
 * accepting a zero-padded minimum width, as in %08x.
 *
 * The format attribute puts this function under the same checking clang
 * applies to the standard printf: a missing argument or one of the wrong type
 * is reported at the call site instead of printing a value nobody passed. */
void kprintf(const char *format, ...)
    __attribute__((format(printf, 1, 2)));

#endif
