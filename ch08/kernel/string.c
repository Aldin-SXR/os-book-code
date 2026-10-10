/* spark -- kernel/string.c
 *
 * Byte-at-a-time implementations: no alignment assumptions and no dependence
 * on the width of a word. A production version would move a word at a time
 * where both addresses allow it.
 */
#include "string.h"

void *memset(void *dst, int value, size_t count)
{
    uint8_t *d = (uint8_t *) dst;
    while (count--) {
        *d++ = (uint8_t) value;
    }
    return dst;
}

void *memcpy(void *dst, const void *src, size_t count)
{
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;
    while (count--) {
        *d++ = *s++;
    }
    return dst;
}

/* memcpy is undefined on overlapping regions and memmove is defined. With the
 * destination above the source, a forward copy would overwrite bytes that
 * have not been read yet, so the copy runs backwards instead. */
void *memmove(void *dst, const void *src, size_t count)
{
    uint8_t *d = (uint8_t *) dst;
    const uint8_t *s = (const uint8_t *) src;

    if (d < s) {
        while (count--) {
            *d++ = *s++;
        }
    } else {
        d += count;
        s += count;
        while (count--) {
            *--d = *--s;
        }
    }
    return dst;
}

int memcmp(const void *a, const void *b, size_t count)
{
    const uint8_t *p = (const uint8_t *) a;
    const uint8_t *q = (const uint8_t *) b;
    while (count--) {
        if (*p != *q) {
            return (int) *p - (int) *q;
        }
        p++; q++;
    }
    return 0;
}

size_t strlen(const char *s)
{
    size_t n = 0;
    while (s[n] != '\0') {
        n++;
    }
    return n;
}
