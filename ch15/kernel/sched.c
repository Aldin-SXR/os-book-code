/* spark -- kernel/sched.c
 *
 * One call on every tick, and the processor stops belonging to whichever
 * process happens to be holding it.
 */
#include "sched.h"
#include "proc.h"
#include "timer.h"

static uint32_t ticks;
static uint32_t preemptions;
static uint32_t slice;
static uint32_t ms_per_tick = 10;
static bool enabled;

/* Make ready every sleeper whose time has come.
 *
 * The comparison is <= rather than ==, because a tick can be missed: this
 * runs with interrupts off, and a process that holds them off for longer
 * than a tick will make the scheduler late rather than making it wrong.
 *
 * The counter wraps after about 497 days at 100 Hz, and spark does not
 * survive that long, so nothing here worries about it. */
static void wake_sleepers(void)
{
    struct process *p = proc_first();

    do {
        if (p->state == PROC_BLOCKED && p->wake_at <= ticks) {
            p->state = PROC_READY;
        }

        p = p->next;
    } while (p != proc_first());
}

/* Every tick, inside the timer's interrupt handler.
 *
 * Everything this does happens with interrupts off, because the gate
 * Chapter 6 installed cleared IF on the way in. That is what makes the
 * counters here safe to touch without a lock, and it is worth noticing that
 * the kernel's own critical section is free while a process's is not. */
static void on_tick(void)
{
    ticks++;
    wake_sleepers();

    if (!enabled) {
        return;
    }

    if (--slice > 0) {
        return;
    }

    slice = SCHED_QUANTUM;
    preemptions++;

    /* And this is the line. Everything above is bookkeeping; this is the
     * one call that makes spark preemptive, and from here on a process can
     * lose the processor between any two of its instructions.
     *
     * It is safe to switch from inside a handler because the frame the stub
     * pushed is on this process's own stack. When it is resumed, it returns
     * through proc_yield into here, out through the dispatcher, and reaches
     * the iret that puts it back where it was interrupted. */
    proc_yield();
}

void sched_init(uint32_t hz)
{
    ticks = 0;
    preemptions = 0;
    slice = SCHED_QUANTUM;
    ms_per_tick = hz > 0 ? 1000u / hz : 10u;
    enabled = true;

    timer_hook(on_tick);
}

uint32_t sched_ticks(void)       { return ticks; }
uint32_t sched_preemptions(void) { return preemptions; }
void sched_disable(void)         { enabled = false; }
void sched_enable(void)          { enabled = true; }

void sched_sleep(uint32_t ms)
{
    struct process *me = proc_current();

    /* Rounded up, so that sleeping for less than a tick still sleeps. */
    uint32_t wait = (ms + ms_per_tick - 1) / ms_per_tick;

    me->wake_at = ticks + wait;
    me->state = PROC_BLOCKED;

    while (me->state == PROC_BLOCKED) {
        proc_yield();

        /* proc_yield came straight back, so nothing else could run. Wait
         * for the tick that will wake us rather than spinning on the test:
         * hlt stops the processor until the next interrupt, which is the
         * timer, which is the thing we are waiting for. */
        if (me->state == PROC_BLOCKED) {
            __asm__ volatile ("hlt");
        }
    }
}
