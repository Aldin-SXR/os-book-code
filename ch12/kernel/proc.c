/* spark -- kernel/proc.c
 *
 * The process list, the stack a new process is given, and the four lines
 * around the switch itself.
 */
#include "proc.h"
#include "heap.h"
#include "vmm.h"
#include "panic.h"
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
    p->directory = (uint32_t *) vmm_directory();
    p->pid = 0;
    p->name = "kmain";
    p->state = PROC_RUNNING;
    p->switches = 0;
    p->stack = NULL;
    p->next = p;

    current = p;
    head = p;
    next_pid = 1;
    process_count = 1;
    switch_count = 0;
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

    *--sp = (uint32_t) proc_exit;   /* entry returns here */
    *--sp = (uint32_t) entry;       /* the ret jumps here */
    *--sp = 0;                      /* ebp */
    *--sp = 0;                      /* ebx */
    *--sp = 0;                      /* esi */
    *--sp = 0;                      /* edi */

    p->esp = (uint32_t) sp;
    p->directory = (uint32_t *) vmm_directory();
    p->pid = next_pid++;
    p->name = name;
    p->state = PROC_READY;
    p->switches = 0;
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
    struct process *from = current;
    struct process *to = next_runnable();

    if (to == from) {
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
    switch_count++;
    current = to;

    /* Everything above this line happens on the outgoing process's stack,
     * and everything after it happens on the incoming one's. The return from
     * this call is a return into the other process. */
    proc_switch(&from->esp, to->esp);
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
