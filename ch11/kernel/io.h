/* spark -- kernel/io.h
 *
 * Byte access to the x86 I/O port space.
 *
 * Device control registers live in a second, 16-bit address space that no
 * pointer can reach: only the in and out instructions address it. C has no
 * syntax for either, so both are wrapped here in inline assembly.
 */
#ifndef SPARK_IO_H
#define SPARK_IO_H

#include "types.h"

/* Send one byte to a port.
 *
 * The constraints carry the instruction's requirements. "a" puts the value in
 * al, the only register outb takes it from. "Nd" puts the port in dx, or
 * encodes it directly when it is a constant below 256. volatile keeps the
 * access: its only effect is on the device, which the compiler cannot see. */
static inline void outb(uint16_t port, uint8_t value)
{
    __asm__ volatile ("outb %0, %1"
                      : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port)
{
    uint8_t value;
    __asm__ volatile ("inb %1, %0"
                      : "=a"(value) : "Nd"(port));
    return value;
}

#endif
