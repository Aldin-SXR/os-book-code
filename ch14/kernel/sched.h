/* spark -- kernel/sched.h
 *
 * The scheduler: the decision Chapter 12 left out.
 *
 * Chapter 12 built the mechanism. A process ran until it called proc_yield,
 * which is enough to share a processor between programs that agree to share
 * it and no use at all against one that does not. Chapter 7 built the timer
 * that interrupts a hundred times a second. This connects the two, and the
 * connection is one call.
 *
 * What that call buys and what it costs are the two halves of Chapter 13.
 * It buys a kernel that cannot be held hostage by a loop. It costs every
 * assumption the kernel had that one thing happens at a time.
 */
#ifndef SPARK_SCHED_H
#define SPARK_SCHED_H

#include "types.h"

/* Ticks a process gets before the timer takes the processor away. At the
 * 100 Hz of Chapter 7 a tick is 10 ms, so this is a 20 ms quantum, which is
 * in the range real systems use and is short enough that the race in
 * Section 13.6 happens while you watch. */
#define SCHED_QUANTUM 2u

/* Claim the timer's hook. Must run after timer_init and proc_init, and after
 * it the processor is no longer any one process's to keep.
 *
 * hz is the rate timer_init was asked for, which is what a sleep in
 * milliseconds has to be converted through. */
void sched_init(uint32_t hz);

/* Ticks since sched_init. The timer's own counter measures the same thing;
 * this one is the scheduler's, and it is what sleeping is measured in. */
uint32_t sched_ticks(void);

/* Give up the processor for at least this many milliseconds. The process is
 * marked PROC_BLOCKED, which is the state Chapter 12 declared and never set,
 * and nothing will run it until the time has passed. */
void sched_sleep(uint32_t ms);

/* How many times the timer has taken the processor away from a process that
 * had not asked to give it up. A voluntary proc_yield does not count. */
uint32_t sched_preemptions(void);

/* Turn preemption off and on again without touching the interrupt flag, so
 * that the timer keeps counting and the keyboard keeps working while one
 * measurement runs undisturbed. */
void sched_disable(void);
void sched_enable(void);

#endif
