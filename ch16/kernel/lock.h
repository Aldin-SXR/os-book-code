/* spark -- kernel/lock.h
 *
 * Mutual exclusion: two ways of making sure that only one thing at a time is
 * part-way through changing something.
 *
 * The problem arrives with preemption and not before. Up to Chapter 12 a
 * process gave up the processor only where it chose to, so it could simply
 * not choose to in the middle of an update. From Chapter 13 the timer takes
 * the processor away between any two instructions, and an update made of
 * more than one instruction can be interrupted half-finished.
 *
 * Two answers are here and they are for different machines.
 *
 * irq_save and irq_restore turn interrupts off. On a machine with one
 * processor that is complete: if nothing can interrupt us, nothing else can
 * run, so nothing else can see the half-finished update. It is also the
 * wrong thing to hand a user program, which could disable interrupts and
 * never enable them again, and Chapter 14 is where that stops being
 * hypothetical.
 *
 * A spinlock is the answer for a machine with more than one processor, where
 * turning our own interrupts off says nothing about what the other processor
 * is doing. spark has one processor and does not need it; it is here because
 * it is the general answer, and because what happens when you use it here is
 * worth seeing.
 */
#ifndef SPARK_LOCK_H
#define SPARK_LOCK_H

#include "types.h"

/* Turn interrupts off and report whether they were on, so that these nest.
 * A function that unconditionally enabled them on the way out would turn
 * them on inside a caller that had turned them off, which is the usual way
 * this goes wrong. */
uint32_t irq_save(void);
void irq_restore(uint32_t flags);

/* True if interrupts are on at this moment. */
bool irq_enabled(void);

/* The general answer, for a machine this book does not have.
 *
 * held is a whole word rather than a bit because xchg works on words, and
 * because there is nothing smaller worth having: the lock is one word either
 * way. */
struct spinlock {
    volatile uint32_t held;
    const char *name;
};

void spin_init(struct spinlock *lock, const char *name);

/* Spin until the lock is ours. On one processor with preemption this is a
 * deadlock waiting to happen, for the reason Chapter 13 measures: the
 * process holding the lock cannot run while we are spinning, because we are
 * the one running. */
void spin_lock(struct spinlock *lock);
void spin_unlock(struct spinlock *lock);

/* How many times spin_lock went round its loop, across all locks. On a
 * machine that never contends this stays at zero. */
uint32_t spin_spins(void);

#endif
