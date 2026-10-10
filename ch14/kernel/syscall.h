/* spark -- kernel/syscall.h
 *
 * The one door in the wall.
 *
 * Chapter 13 gave spark processes that share a processor. They still shared
 * everything else: a process could write to the video buffer, read the page
 * tables, or execute cli and stop the machine. This chapter takes all of
 * that away and gives back a list of things a process may ask for.
 *
 * A system call is not a function call and the difference is not style. A
 * call goes to an address the caller chose; this goes to an address the
 * kernel chose, through a gate the processor checks, and it changes the ring
 * on the way. The user side cannot name syscall_dispatch and cannot jump to
 * it: it can only raise interrupt 0x80 and be sent there.
 */
#ifndef SPARK_SYSCALL_H
#define SPARK_SYSCALL_H

#include "types.h"
#include "isr.h"

/* Linux picked 0x80 for the same reason we do: it is well clear of the 32
 * vectors the processor reserves and of the 16 the interrupt controller was
 * remapped onto in Chapter 6. */
#define SYSCALL_VECTOR 0x80

/* The calls spark offers. The number goes in eax, the arguments in ebx, ecx
 * and edx, and the result comes back in eax. That is a calling convention
 * like cdecl, except that it is between two rings rather than two functions,
 * so it has to use registers: the kernel cannot read the user's stack
 * without first deciding whether to trust the pointer to it. */
#define SYS_WRITE  1   /* ebx: text, ecx: length. Returns bytes written. */
#define SYS_EXIT   2   /* ebx: status. Does not return.                  */
#define SYS_GETPID 3   /* Returns the caller's pid.                      */
#define SYS_YIELD  4   /* Gives up the rest of the quantum. Returns 0.   */
#define SYS_SLEEP  5   /* ebx: milliseconds. Returns 0.                  */

/* What a refused call returns, in eax. Chosen to be what -1 looks like to a
 * program reading it as a signed number. */
#define SYS_ERROR 0xFFFFFFFFu

/* Claim vector 0x80. Must run after idt_init. */
void syscall_init(void);

/* How many calls have been made, and how many of those were refused because
 * the kernel would not do what they asked. */
uint32_t syscall_count(void);
uint32_t syscall_refused(void);

/* The cs and ss of the first caller, taken out of its own interrupt frame.
 * Read rather than assumed: these are what the processor saved on the way
 * in, so they say what ring the caller was really running at. */
uint32_t syscall_user_cs(void);
uint32_t syscall_user_ss(void);

#endif
