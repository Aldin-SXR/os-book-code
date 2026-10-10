/* spark -- kernel/lock.c
 *
 * Two locks: one that works on this machine, and one that would work on a
 * machine with more processors than this one has.
 */
#include "lock.h"

#define EFLAGS_IF (1u << 9)

static uint32_t spins;

static uint32_t read_eflags(void)
{
    uint32_t flags;

    __asm__ volatile ("pushf; pop %0" : "=r" (flags));
    return flags;
}

bool irq_enabled(void)
{
    return (read_eflags() & EFLAGS_IF) != 0;
}

uint32_t irq_save(void)
{
    uint32_t flags = read_eflags();

    __asm__ volatile ("cli" ::: "memory");
    return flags;
}

void irq_restore(uint32_t flags)
{
    /* Put them back the way they were rather than simply enabling them. A
     * caller that had already turned them off gets them left off, which is
     * what makes these safe to nest. */
    if (flags & EFLAGS_IF) {
        __asm__ volatile ("sti" ::: "memory");
    }
}

void spin_init(struct spinlock *lock, const char *name)
{
    lock->held = 0;
    lock->name = name;
}

/* Write 1 into the lock and report what was there before, in one instruction
 * the hardware will not divide.
 *
 * xchg is the whole of it. Without an instruction that reads and writes in
 * one indivisible step there is no way to build a lock at all: a test
 * followed by a set can be interrupted between the two, and then two
 * processes have both seen the lock free and both taken it. On x86 xchg with
 * a memory operand is atomic whether or not it is written with a lock
 * prefix, which is the one place the architecture makes that promise
 * silently. */
static uint32_t test_and_set(volatile uint32_t *word)
{
    uint32_t value = 1;

    __asm__ volatile ("xchg %0, %1"
                      : "+r" (value), "+m" (*word)
                      :: "memory");
    return value;
}

void spin_lock(struct spinlock *lock)
{
    while (test_and_set(&lock->held) != 0) {
        spins++;
    }
}

void spin_unlock(struct spinlock *lock)
{
    /* A plain store is enough to release on this processor, because x86
     * does not move a store before an earlier store. */
    __asm__ volatile ("" ::: "memory");
    lock->held = 0;
}

uint32_t spin_spins(void)
{
    return spins;
}
