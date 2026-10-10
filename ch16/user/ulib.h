/* spark -- user/ulib.h
 *
 * What a program on the disk has instead of a standard library.
 *
 * There is no libc here and there is nowhere to get one. Everything a
 * program can do that has any effect outside its own memory is a system
 * call, and everything else it needs it brings with it. That is not a
 * limitation of spark: it is what a standard library is, once you take away
 * the part somebody already compiled for you.
 */
#ifndef SPARK_ULIB_H
#define SPARK_ULIB_H

#include "../kernel/sysnum.h"

typedef unsigned int u32;

/* The three shapes of system call, differing only in how many registers
 * they set. There are no default arguments in C and a variadic version
 * would need a stack the kernel would have to trust. */
u32 call0(u32 number);
u32 call1(u32 number, u32 a);
u32 call2(u32 number, u32 a, u32 b);
u32 call3(u32 number, u32 a, u32 b, u32 c);

/* The five a program actually uses, named so that the programs read like
 * programs rather than like a table of numbers. */
void exit(u32 status) __attribute__((noreturn));
u32 write(const char *text, u32 len);
u32 read_file(const char *path, char *buf, u32 size);
u32 list(u32 index, char *name);
u32 getpid(void);

/* And the four pieces of a standard library small enough to carry. */
u32 ustrlen(const char *s);
void puts(const char *s);
void putn(u32 value);

#endif
