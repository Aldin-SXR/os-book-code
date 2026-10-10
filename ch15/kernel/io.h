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

/* A word at a time, which is how a disk moves data.
 *
 * The ATA data register is sixteen bits wide and a sector is 256 reads of
 * it. Doing that as 512 byte reads would work on this controller and is not
 * what the interface says, so the width is part of the protocol rather than
 * a choice. insw and outsw repeat the transfer ecx times and advance the
 * pointer, so one instruction moves the whole sector. */
static inline void insw(uint16_t port, void *to, uint32_t n)
{
    __asm__ volatile ("rep insw"
                      : "+D"(to), "+c"(n)
                      : "d"(port) : "memory");
}

static inline void outsw(uint16_t port, const void *from,
                         uint32_t n)
{
    __asm__ volatile ("rep outsw"
                      : "+S"(from), "+c"(n)
                      : "d"(port) : "memory");
}

#endif
