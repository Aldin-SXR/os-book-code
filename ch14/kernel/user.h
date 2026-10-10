/* spark -- kernel/user.h
 *
 * The part of the address space ring 3 is allowed to touch, and the check
 * that decides whether a pointer it hands us is inside it.
 *
 * Chapter 11 built page tables and never set PAGE_USER on anything, so every
 * page spark has mapped so far is a supervisor page: reachable from ring 0
 * and from nowhere else. That bit is the second half of the wall. The first
 * half is the ring in cs, which stops a user program executing privileged
 * instructions; this half stops it reading the kernel's memory.
 */
#ifndef SPARK_USER_H
#define SPARK_USER_H

#include "types.h"

/* Where user stacks go. Well above the direct map of Chapter 11 and well
 * below the heap at 0xC0000000, so it collides with neither. */
#define USER_STACK_BASE  0x40000000u
#define USER_STACK_BYTES 4096u
#define USER_STACKS      4

/* Mark the pages of the .user section readable from ring 3. Must run after
 * vmm_init, because it edits the tables that call built. */
void user_init(void);

/* Hand out one user stack and return the address one past its top, which is
 * what esp should be set to. Returns 0 when they have all been handed out. */
uint32_t user_stack_alloc(void);

/* Is every byte of this range something ring 3 is allowed to read?
 *
 * This is the check a kernel cannot skip. A user program passes a number and
 * calls it a pointer; the kernel runs with the privilege to follow it
 * anywhere, so it has to decide for itself whether the number names memory
 * that program was allowed to see. */
bool user_may_read(uint32_t addr, uint32_t len);

/* The bounds of the user section, for printing. */
uint32_t user_text_base(void);
uint32_t user_text_end(void);

#endif
