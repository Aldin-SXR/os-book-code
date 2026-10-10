/* spark -- kernel/types.h
 *
 * Fixed-width types for the whole kernel, and the target assumptions the
 * rest of the code is built on.
 *
 * A freestanding compiler still supplies the headers that describe the
 * language rather than the system. These three are the ones spark needs.
 */
#ifndef SPARK_TYPES_H
#define SPARK_TYPES_H

#include <stdint.h>   /* uint8_t, uint32_t, uintptr_t */
#include <stddef.h>   /* size_t, NULL, offsetof */
#include <stdbool.h>  /* bool, true, false */

/* spark is 32-bit throughout: four-byte addresses and a four-byte size_t are
 * assumed by every hardware structure in the kernel. Asserting it here turns
 * a wrong target into a compile error rather than a kernel that boots and
 * then misreads every descriptor it builds. */
_Static_assert(sizeof(void *) == 4,
               "spark is a 32-bit kernel");
_Static_assert(sizeof(size_t) == 4,
               "spark is a 32-bit kernel");
_Static_assert(sizeof(uint32_t) == 4, "uint32_t must be 4");

#endif
