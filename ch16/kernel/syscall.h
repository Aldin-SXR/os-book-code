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
#include "sysnum.h"

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
