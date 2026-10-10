/* spark -- kernel/sysnum.h
 *
 * The numbers, and nothing else.
 *
 * Both sides of the boundary need this list and they are compiled
 * separately: the kernel is linked into spark.elf, and the programs in
 * user/ are linked into flat binaries that live on the disk. Two copies of
 * a list of numbers is how a call ends up meaning one thing to the caller
 * and another to the kernel, so there is one copy and it is here.
 */
#ifndef SPARK_SYSNUM_H
#define SPARK_SYSNUM_H

#define SYS_WRITE  1   /* ebx text, ecx length. Returns n. */
#define SYS_EXIT   2   /* ebx status. Does not return. */
#define SYS_GETPID 3   /* Returns the caller's pid. */
#define SYS_YIELD  4   /* Gives up the quantum. Returns 0. */
#define SYS_SLEEP  5   /* ebx milliseconds. Returns 0. */
#define SYS_READ   6   /* ebx path, ecx buf, edx size. */
#define SYS_LIST   7   /* ebx index, ecx name. -> inode. */

/* What a refused call returns, in eax. Chosen to be what -1 looks like to a
 * program reading it as a signed number. */
#define SYS_ERROR 0xFFFFFFFFu

/* Linux picked 0x80 for the same reason we do: it is well clear of the 32
 * vectors the processor reserves and of the 16 the interrupt controller was
 * remapped onto in Chapter 6. */
#define SYSCALL_VECTOR 0x80

#endif
