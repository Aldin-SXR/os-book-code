/* spark -- kernel/proc.c
 *
 * The process list, the stack a new process is given, and the four lines
 * around the switch itself.
 */
#include "proc.h"
#include "heap.h"
#include "lock.h"
#include "vmm.h"
#include "panic.h"
#include "tss.h"
#include "user.h"
#include "string.h"

/* In switch.asm. The first argument is where to write the outgoing stack
 * pointer and the second is the incoming one. */
void proc_switch(uint32_t *save_to, uint32_t load_from);

static struct process *current;
static struct process *head;   /* process 0, so walks start there */
static uint32_t next_pid;
static uint32_t process_count;
static uint32_t switch_count;

const char *proc_state_name(enum proc_state state)
{
    switch (state) {
    case PROC_READY:   return "ready";
    case PROC_RUNNING: return "running";
    case PROC_BLOCKED: return "blocked";
    case PROC_DONE:    return "done";
    default:           return "?";
    }
}

void proc_init(void)
{
    struct process *p = kmalloc(sizeof *p);

    if (p == NULL) {
        panic("proc_init: no room for the first process");
    }

    /* Process 0 is the code that is already running, so there is nothing to
     * set up: it has a stack, it has registers, and its esp will be written
     * down by the first switch away from it. The one field that would be
     * wrong is the stack, because boot.asm reserved that one and the heap
     * must never be asked to free it. */
    p->esp = 0;
    p->entry = NULL;
    p->directory = (uint32_t *) vmm_directory();
    p->pid = 0;

    memcpy(p->label, "kmain", 6);
    p->name = p->label;

    p->state = PROC_RUNNING;
    p->switches = 0;
    p->wake_at = 0;
    p->stack = NULL;
    p->next = p;

    current = p;
    head = p;
    next_pid = 1;
    process_count = 1;
    switch_count = 0;
}

/* Where every process actually begins.
 *
 * A process that has never run is resumed by proc_switch's ret, and from
 * Chapter 13 that resume can happen from inside the timer's interrupt
 * handler, where the gate cleared IF on the way in. An ordinary process
 * gets its interrupts back from the iret at the end of the handler; a
 * process starting for the first time never reaches one, because it starts
 * at its entry point rather than returning to where it was interrupted.
 *
 * So it turns them on itself, here, once. Without this line the first
 * process created after sched_init runs with interrupts off for ever, the
 * timer stops, and the machine hangs in whatever loop that process is in.
 *
 * Returning from here lands on proc_exit, which proc_create put on the
 * stack above this function exactly as Chapter 12 described. */
static void proc_start(void)
{
    __asm__ volatile ("sti");
    current->entry();
}

struct process *proc_create(const char *name, void (*entry)(void))
{
    struct process *p = kmalloc(sizeof *p);

    if (p == NULL) {
        return NULL;
    }

    uint8_t *stack = kmalloc(PROC_STACK_BYTES);

    if (stack == NULL) {
        kfree(p);
        return NULL;
    }

    /* Every field, including the ones set below and the ones only some
     * kinds of process use. kmalloc hands back whatever the last owner of
     * that block left, and a control block with a stale killed pointer in
     * it describes a process that died of something that never happened. */
    memset(p, 0, sizeof *p);

    /* Build the stack this process will be resumed from, so that the first
     * resume is the same operation as every one after it. proc_switch ends
     * in a ret, so whatever it finds at the top is where the process starts.
     *
     * The stack grows down, so the words go in from the top of the block.
     * From the top: the address proc_exit, which is where the entry function
     * returns to if it ever returns; then the entry point itself, which the
     * ret jumps to; then four zeros standing in for the registers a real
     * switch would have pushed. */
    uint32_t *sp = (uint32_t *) (stack + PROC_STACK_BYTES);

    *--sp = (uint32_t) proc_exit;   /* back here at the end */
    *--sp = (uint32_t) proc_start;  /* the ret jumps here */
    *--sp = 0;                      /* ebp */
    *--sp = 0;                      /* ebx */
    *--sp = 0;                      /* esi */
    *--sp = 0;                      /* edi */

    p->esp = (uint32_t) sp;
    p->entry = entry;
    p->directory = (uint32_t *) vmm_directory();
    p->pid = next_pid++;

    uint32_t n = 0;

    while (n + 1 < PROC_NAME_MAX && name[n] != '\0') {
        p->label[n] = name[n];
        n++;
    }

    p->label[n] = '\0';
    p->name = p->label;
    p->state = PROC_READY;
    p->switches = 0;
    p->wake_at = 0;
    p->stack = stack;

    /* Onto the end of the ring, so that a walk from process 0 meets them in
     * the order they were made. The ring has no end, so the end is the one
     * whose next is the beginning. */
    struct process *last = head;

    while (last->next != head) {
        last = last->next;
    }

    p->next = head;
    last->next = p;
    process_count++;

    return p;
}

/* The next process round the ring that could use the processor. Returns the
 * caller itself when nothing else can, which is what keeps a lone process
 * running rather than leaving the kernel with nobody to switch to. */
static struct process *next_runnable(void)
{
    struct process *p = current->next;

    while (p != current) {
        if (p->state == PROC_READY) {
            return p;
        }

        p = p->next;
    }

    return current;
}

void proc_yield(void)
{
    /* The scheduler's own list is shared, and from Chapter 13 the timer can
     * interrupt this function half-way through and call it again. A second
     * call that ran here would find current already moved, switch away from
     * a process that is no longer running, and lose one for ever.
     *
     * So this is a critical section, and it is the first one in the kernel.
     * The flags are a local, which means they live on this process's stack
     * and come back with it: the restore at the end runs in whatever
     * process is resumed here, putting that process's interrupt flag back
     * rather than this one's. */
    uint32_t flags = irq_save();

    struct process *from = current;
    struct process *to = next_runnable();

    if (to == from) {
        irq_restore(flags);
        return;
    }

    /* The outgoing one goes back in the queue unless it is finished. Doing
     * this before the switch matters: after it, this function is not
     * running any more and will not run again until somebody switches back
     * to it. */
    if (from->state == PROC_RUNNING) {
        from->state = PROC_READY;
    }

    to->state = PROC_RUNNING;
    to->switches++;

    /* Where the processor will land if this process is interrupted while it
     * is in ring 3. It has to be reset on every switch, because each process
     * has a kernel stack of its own and the processor only knows about one
     * at a time. Getting this wrong puts two processes' interrupt frames on
     * the same stack, which Section 14.9 measures. */
    if (to->stack != NULL) {
        tss_set_stack((uint32_t) (uintptr_t) to->stack +
                      PROC_STACK_BYTES);
    }
    switch_count++;
    current = to;

    /* Everything above this line happens on the outgoing process's stack,
     * and everything after it happens on the incoming one's. The return from
     * this call is a return into the other process. */
    proc_switch(&from->esp, to->esp);

    /* Back, some time later, in the process that saved those flags. */
    irq_restore(flags);
}

/* Defined in user_enter.asm: there is no C expression for iret. */
extern void user_enter(void (*entry)(void),
                       uint32_t stack_top);

/* What a user process runs in ring 0, for the few instructions it is there.
 *
 * It exists because proc_create takes a function of no arguments, and
 * because the drop to ring 3 needs two addresses that the scheduler has no
 * way of passing. Both are in the process control block, so this reads them
 * from there and never comes back. */
static void user_trampoline(void)
{
    struct process *me = current;

    if (me->user_stack == 0) {
        panic("user_trampoline: no stack for ring 3");
    }

    user_enter(me->user_entry, me->user_stack);
}

struct process *proc_create_program(const char *name, uint32_t entry,
                                    uint32_t stack_top)
{
    struct process *p = proc_create(name, user_trampoline);

    if (p == NULL) {
        return NULL;
    }

    p->user_entry = (void (*)(void)) (uintptr_t) entry;
    p->user_stack = stack_top;
    return p;
}

struct process *proc_create_user(const char *name,
                                 void (*fn)(void))
{
    uint32_t stack = user_stack_alloc();

    if (stack == 0) {
        return NULL;
    }

    struct process *p = proc_create(name, user_trampoline);

    if (p == NULL) {
        return NULL;
    }

    p->user_entry = fn;
    p->user_stack = stack;
    return p;
}

void proc_kill(const char *why, uint32_t error,
               uint32_t address)
{
    current->killed = why;
    current->exit_status = error;
    current->fault_addr = address;
    current->state = PROC_DONE;

    /* The faulting instruction is never reached again, and neither is the
     * iret that would have returned to it. The kernel stack this is
     * standing on is abandoned along with the process. */
    for (;;) {
        proc_yield();
    }
}

void proc_exit(void)
{
    current->state = PROC_DONE;

    /* The stack is not freed here, because we are standing on it. A kernel
     * that reaps its processes does it from somewhere else, and spark's
     * processes last as long as the kernel does. */
    for (;;) {
        proc_yield();
    }
}

struct process *proc_current(void) { return current; }
struct process *proc_first(void)   { return head;    }
uint32_t proc_count(void)          { return process_count; }
uint32_t proc_switches(void)       { return switch_count; }
