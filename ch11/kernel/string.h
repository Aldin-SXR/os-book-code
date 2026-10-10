/* spark -- kernel/string.h
 *
 * The four memory functions a freestanding build must supply, plus strlen.
 *
 * The names and signatures are the standard ones by necessity: the compiler
 * emits calls to memcpy, memset, memmove and memcmp from code that never
 * names them, and resolves them by exactly these symbols.
 */
#ifndef SPARK_STRING_H
#define SPARK_STRING_H

#include "types.h"

void  *memset(void *dst, int value, size_t count);
void  *memcpy(void *dst, const void *src, size_t count);
void  *memmove(void *dst, const void *src, size_t count);
int    memcmp(const void *a, const void *b, size_t count);
size_t strlen(const char *s);

#endif
