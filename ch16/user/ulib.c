/* spark -- user/ulib.c
 *
 * Seven lines of assembly and four of C.
 */
#include "ulib.h"

u32 call0(u32 number)
{
    u32 result;

    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "a" (number)
                      : "memory");
    return result;
}

u32 call1(u32 number, u32 a)
{
    u32 result;

    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "a" (number), "b" (a)
                      : "memory");
    return result;
}

u32 call2(u32 number, u32 a, u32 b)
{
    u32 result;

    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "a" (number), "b" (a), "c" (b)
                      : "memory");
    return result;
}

u32 call3(u32 number, u32 a, u32 b, u32 c)
{
    u32 result;

    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "a" (number), "b" (a),
                        "c" (b), "d" (c)
                      : "memory");
    return result;
}

void exit(u32 status)
{
    call1(SYS_EXIT, status);

    /* SYS_EXIT does not return, and the compiler has to be told twice:
     * once by the attribute on the declaration, and once by a loop here,
     * because it cannot see inside the interrupt. */
    for (;;) {
    }
}

u32 write(const char *text, u32 len)
{
    return call2(SYS_WRITE, (u32) text, len);
}

u32 read_file(const char *path, char *buf, u32 size)
{
    return call3(SYS_READ, (u32) path, (u32) buf, size);
}

u32 list(u32 index, char *name)
{
    return call2(SYS_LIST, index, (u32) name);
}

u32 getpid(void)
{
    return call0(SYS_GETPID);
}

u32 ustrlen(const char *s)
{
    u32 n = 0;

    while (s[n] != '\0') {
        n++;
    }
    return n;
}

void puts(const char *s)
{
    write(s, ustrlen(s));
}

/* Numbers, without a printf.
 *
 * The digits come out backwards, which is why this fills a buffer from the
 * end rather than the start. Twelve bytes is enough for any 32-bit value
 * and the terminator. */
void putn(u32 value)
{
    char buf[12];
    u32 at = sizeof buf;

    buf[--at] = '\0';

    do {
        buf[--at] = (char) ('0' + value % 10);
        value /= 10;
    } while (value != 0);

    puts(&buf[at]);
}
