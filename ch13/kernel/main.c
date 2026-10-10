/* spark -- kernel/main.c
 *
 * Two halves, and the second is a consequence of the first. Preemption makes
 * the machine fair; preemption also makes a three-line update into something
 * that can be interrupted half-finished, and the same screen shows both.
 */
#include "types.h"
#include "vga.h"
#include "printf.h"
#include "gdt.h"
#include "idt.h"
#include "isr.h"
#include "pic.h"
#include "panic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "vmm.h"
#include "heap.h"
#include "proc.h"
#include "sched.h"
#include "lock.h"

#define RULE_WIDTH 62
#define HZ         100

/* Increments each of the two workers performs. Large enough that a 20 ms
 * quantum lands inside the update many times over. */
#define ROUNDS 400000u

/* How long to stay between the read and the write. A race is not reliable,
 * and a demonstration that fails one run in ten teaches nothing, so the
 * window is widened on purpose until the bug is certain. Everything about
 * it is real except the size of the gap. */
#define WINDOW 40u

/* Iterations used to find out how fast this machine is. Big enough that the
 * answer is not one tick wide. */
#define CALIBRATION 40000000u

/* What the two workers share, and how they change it.
 *
 * The three steps are written out rather than left as counter++ for a
 * reason the chapter measures: counter++ on a volatile compiles to one
 * instruction, and one instruction cannot be interrupted half-way. Written
 * like this it is a read, some arithmetic, and a write, which is what every
 * update looks like on a machine the compiler cannot fuse it for, and what
 * every update looks like on a machine with two processors. */
static volatile uint32_t counter;
static volatile uint32_t atomic_counter;

/* Whether the workers protect the update, set before they are started. */
static volatile bool use_lock;

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
}

/* kprintf pads numbers and not strings. */
static void pad(const char *s, uint32_t width)
{
    uint32_t n = 0;

    while (s[n] != '\0') {
        vga_putchar(s[n++]);
    }

    while (n++ < width) {
        vga_putchar(' ');
    }
}

/* Not static, and not inlinable, so that the read and the write really are
 * separated by a call the compiler cannot see through. */
uint32_t widen(uint32_t v);

uint32_t widen(uint32_t v)
{
    for (volatile uint32_t i = 0; i < WINDOW; i++) {
        /* nothing, on purpose */
    }

    return v;
}

static void bump(void)
{
    uint32_t seen = counter;     /* read  */

    seen = widen(seen);     /* and something in between */

    counter = seen + 1;          /* write */
}

static void worker(void)
{
    for (uint32_t i = 0; i < ROUNDS; i++) {
        if (use_lock) {
            uint32_t flags = irq_save();

            bump();
            irq_restore(flags);
        } else {
            bump();
        }

        /* One instruction, for comparison. */
        atomic_counter++;
    }
}

/* Three processes of different lengths, to show what round robin does to
 * a short job that arrives behind a long one. Each records when it first
 * ran and when it finished. */
struct job {
    const char *name;
    uint32_t work_ms;
    uint32_t first_tick;
    uint32_t done_tick;
};

static struct job jobs[3] = {
    { "long",   200, 0, 0 },
    { "medium",  50, 0, 0 },
    { "short",    5, 0, 0 },
};

static volatile uint32_t job_index;

/* Work rather than waiting.
 *
 * A job that spun until the clock reached some value would not be competing
 * for anything: it would finish at the same moment however much processor it
 * was given. These jobs count instead, so a job that is given half the
 * processor takes twice as long, which is the whole point of the table. */
static volatile uint32_t sink;

static void do_work(uint32_t iterations)
{
    for (uint32_t i = 0; i < iterations; i++) {
        sink = sink + 1;
    }
}

/* Iterations of do_work that take a millisecond on this machine, measured
 * here rather than guessed, because it depends on the host. */
static uint32_t per_ms;

static void calibrate(void)
{
    /* With preemption off and nothing else runnable, the only thing that
     * can interrupt the loop is the timer, which is what is being read. */
    sched_disable();

    uint32_t mark = timer_uptime_ms();

    while (timer_uptime_ms() == mark) {
        /* line the measurement up with a tick */
    }

    mark = timer_uptime_ms();
    do_work(CALIBRATION);

    uint32_t took = timer_uptime_ms() - mark;

    per_ms = took > 0 ? CALIBRATION / took : CALIBRATION;
    sched_enable();
}

static void burn(uint32_t ms)
{
    do_work(ms * per_ms);
}

static void run_job(void)
{
    struct job *j = &jobs[job_index++];

    j->first_tick = sched_ticks();
    burn(j->work_ms);
    j->done_tick = sched_ticks();
}

static uint32_t ms_of(uint32_t ticks)
{
    return ticks * (1000u / HZ);
}

/* How long the sleeper asks for, and what it measured. */
#define SLEEP_MS 50u

static volatile uint32_t slept_ms;

static void sleeper(void)
{
    uint32_t mark = sched_ticks();

    sched_sleep(SLEEP_MS);
    slept_ms = ms_of(sched_ticks() - mark);
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 13: scheduling and concurrency\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    idt_init();
    pic_init(IRQ(0), IRQ(8));
    timer_init(HZ);
    kbd_init();
    __asm__ volatile ("sti");

    pmm_init(magic, boot_info);
    vmm_init();
    heap_init();
    proc_init();
    sched_init(HZ);

    kprintf("\nquantum       %u ticks, %u ms, and the timer now enforces it\n",
            SCHED_QUANTUM, ms_of(SCHED_QUANTUM));
    rule();

    calibrate();
    kprintf("\nthis machine  %u thousand turns of the work loop per ms\n",
            per_ms / 1000u);
    rule();

    /* --- half one: three jobs, and what round robin does to them --- */
    uint32_t start = sched_ticks();

    for (uint32_t i = 0; i < 3; i++) {
        if (proc_create(jobs[i].name, run_job) == NULL) {
            panic("kmain: no room for a job");
        }
    }

    while (jobs[0].done_tick == 0 || jobs[1].done_tick == 0 ||
           jobs[2].done_tick == 0) {
        proc_yield();
    }

    kprintf("\nthree jobs, all ready at once, round robin:\n");
    kprintf("  name    work     response   turnaround\n");

    for (uint32_t i = 0; i < 3; i++) {
        kprintf("  ");
        pad(jobs[i].name, 8);
        kprintf("%3u ms %8u ms %9u ms\n",
                jobs[i].work_ms,
                ms_of(jobs[i].first_tick - start),
                ms_of(jobs[i].done_tick - start));
    }

    kprintf("  first come first served: response 0, %u, %u ms\n",
            jobs[0].work_ms, jobs[0].work_ms + jobs[1].work_ms);

    /* --- half two: the same preemption, now a problem --- */
    kprintf("\ntwo processes, %u increments each, expecting %u:\n",
            ROUNDS, 2u * ROUNDS);

    counter = 0;
    atomic_counter = 0;
    use_lock = false;

    uint32_t before = sched_preemptions();

    struct process *wa = proc_create("bump-a", worker);
    struct process *wb = proc_create("bump-b", worker);

    if (wa == NULL || wb == NULL) {
        panic("kmain: no room for a worker");
    }

    /* Wait for both to finish rather than for a counter to reach a value.
     * The counter is exactly the thing under suspicion here, so a loop that
     * trusted it would be reporting on its own bug. */
    while (wa->state != PROC_DONE || wb->state != PROC_DONE) {
        proc_yield();
    }

    kprintf("  read, work, write   %u, and %u lost\n",
            counter, 2u * ROUNDS - counter);
    kprintf("  one instruction     %u, which is all of them\n",
            atomic_counter);
    kprintf("  and the timer interrupted them %u times\n",
            sched_preemptions() - before);
    rule();

    kprintf("\nwith interrupts off around the three lines:\n");

    counter = 0;
    atomic_counter = 0;
    use_lock = true;
    before = sched_preemptions();

    wa = proc_create("lock-a", worker);
    wb = proc_create("lock-b", worker);

    if (wa == NULL || wb == NULL) {
        panic("kmain: no room for a worker");
    }

    while (wa->state != PROC_DONE || wb->state != PROC_DONE) {
        proc_yield();
    }

    kprintf("  read, work, write   %u, and %u lost\n",
            counter, 2u * ROUNDS - counter);
    kprintf("  interrupted %u times anyway, between the sections\n",
            sched_preemptions() - before);
    rule();

    /* --- and the state Chapter 12 declared and never set --- */
    struct process *sl = proc_create("sleeper", sleeper);

    if (sl == NULL) {
        panic("kmain: no room for a sleeper");
    }

    while (sl->state != PROC_BLOCKED && sl->state != PROC_DONE) {
        proc_yield();
    }

    const char *saw = proc_state_name(sl->state);

    while (sl->state != PROC_DONE) {
        __asm__ volatile ("hlt");
    }

    kprintf("\nsleeping      asked for %u ms, was %s, slept %u ms\n",
            SLEEP_MS, saw, slept_ms);
    kprintf("              and the processor was halted, not spinning\n");
    rule();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
