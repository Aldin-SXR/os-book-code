/* spark -- kernel/proc.h
 *
 * Processes: several programs in memory at once, and the machinery for
 * taking the processor away from one and giving it to another.
 *
 * A process is not a program. The program is the instructions, which sit
 * still; the process is the program plus everything the machine is holding
 * on its behalf, which is the registers, the stack and the address space.
 * Two processes can run the same program, and the thing that makes them two
 * is that each has its own copy of all of that.
 *
 * What spark has here shares one address space with the kernel and runs in
 * ring 0, which by the usual naming makes each one a kernel thread rather
 * than a process proper. The structure below is the process structure all
 * the same, because Chapter 14 turns these into processes by adding the two
 * things they are missing, and nothing about the switching changes when it
 * does.
 *
 * Switching here is voluntary: a process runs until it calls proc_yield.
 * Chapter 13 takes that choice away from it.
 */
#ifndef SPARK_PROC_H
#define SPARK_PROC_H

#include "types.h"

/* Each process gets its own stack, out of the heap. 4 KiB is generous for
 * what these do and small enough that a few dozen of them cost little. */
#define PROC_STACK_BYTES 4096u

/* How much of a process's name is kept. The name is copied rather than
 * pointed at, because from Chapter 16 it comes from a command line: a
 * buffer on the shell's stack that is gone by the time anybody prints the
 * process table. A pointer to it survives long enough to look correct. */
#define PROC_NAME_MAX 16

/* The states of the diagram every operating systems text draws.
 *
 * PROC_BLOCKED is declared and never set. A process blocks when it asks for
 * something that is not ready yet, and spark has nothing to ask for until
 * Chapter 13 gives it sleeping and Chapter 15 gives it a disk. It is here so
 * that the enumeration is the real one rather than a subset. */
enum proc_state {
    PROC_READY,     /* could run, is not running      */
    PROC_RUNNING,   /* on the processor; only one is  */
    PROC_BLOCKED,   /* waiting; unused until ch. 13   */
    PROC_DONE       /* returned from its entry point  */
};

/* The process control block: everything the kernel knows about one process.
 *
 * On a real system this is the larger structure in the kernel, holding open
 * files, a parent, accounting, signal handlers and more. Every one of those
 * is a later chapter, and the four fields that matter now are the ones that
 * make a process distinct from the program it is running. */
struct process {
    /* Where this process's stack pointer was when it last gave up the
     * processor. Everything else it was holding is on that stack, which is
     * why this one word is the whole of a saved context here. */
    uint32_t esp;

    /* Its address space. Every process points at the same directory today,
     * because they share one, and the field is here because the switch is
     * where a real kernel reloads cr3. */
    uint32_t *directory;

    uint32_t pid;

    /* Points at label below, always. The separate pointer is kept so that
     * everything reading a process's name still reads a const char *. */
    const char *name;
    char label[PROC_NAME_MAX];
    enum proc_state state;

    /* How many times this process has been given the processor. */
    uint32_t switches;

    /* The tick at which a sleeping process should be made ready again.
     * Meaningless unless state is PROC_BLOCKED. Chapter 12 declared that
     * state and never set it; Chapter 13 is where it starts being used. */
    uint32_t wake_at;

    /* What this process runs. proc_create does not put this address on the
     * stack directly any more: Chapter 13 puts a trampoline there instead,
     * and the trampoline reads this. */
    void (*entry)(void);

    /* The block proc_create took from the heap, kept so that it can be
     * given back. The stack grows down from the top of it. */
    void *stack;

    /* What a user process runs once it reaches ring 3, and the stack it
     * runs on there. Both are zero for a kernel process. Chapter 14 needs
     * two entry points rather than one, because the address the trampoline
     * jumps to and the address iret returns to are in different rings. */
    void (*user_entry)(void);
    uint32_t user_stack;

    /* Where this process's SYS_WRITE goes: 0 for the screen, or the inode
     * of a file to append to. The shell sets it before the program starts,
     * which is the whole of redirection. */
    uint32_t out_inode;

    /* What the process made of itself: the status it passed to SYS_EXIT,
     * or the fault that killed it. killed is NULL for a process that ended
     * on its own terms. */
    uint32_t exit_status;
    const char *killed;

    /* The address a killed process was reaching for, from cr2. Meaningless
     * unless killed names a page fault. */
    uint32_t fault_addr;

    /* The process list is circular, so that finding the next one to run is
     * following a pointer and the last one leads back to the first. */
    struct process *next;
};

/* Turn the code already running into process 0, so that there is something
 * to switch away from. Must run after heap_init. */
void proc_init(void);

/* A new process running entry, with a fresh stack. Returns NULL if the heap
 * could not supply the block or the stack. The process is READY, and nothing
 * runs it until somebody yields. */
struct process *proc_create(const char *name, void (*entry)(void));

/* Give up the processor to the next process that could use it. Returns when
 * somebody yields back, which may be a long time and may be never. */
void proc_yield(void);

/* A process that begins in ring 0, like any other, and drops to ring 3 as
 * its first act. fn is the ring-3 entry point and has to be in the .user
 * section; calling it from the kernel would run it with every privilege it
 * is meant not to have. */
struct process *proc_create_user(const char *name,
                                 void (*fn)(void));

/* A process running a program that was loaded from the disk. Unlike
 * proc_create_user this allocates no user stack: the loader has already
 * built one with the program's arguments on it, and passes the stack
 * pointer the program should start with. */
struct process *proc_create_program(const char *name, uint32_t entry,
                                    uint32_t stack_top);

/* End the calling process because it did something it was not allowed to
 * do, recording what that was. Called from a fault handler, so it never
 * returns to the instruction that faulted. */
void proc_kill(const char *why, uint32_t error, uint32_t address)
    __attribute__((noreturn));

/* End the calling process. It is marked PROC_DONE, its stack is left alone
 * because it is still standing on it, and it yields away for the last time.
 * Never returns. A process that returns from its entry function arrives here
 * anyway, because proc_create arranges for it. */
void proc_exit(void) __attribute__((noreturn));

/* The one that is running. Never NULL after proc_init. */
struct process *proc_current(void);

/* The head of the circular list, for walking it. */
struct process *proc_first(void);

/* How many processes exist, and how many switches have happened in all. */
uint32_t proc_count(void);
uint32_t proc_switches(void);

/* "ready", "running", "blocked", "done". */
const char *proc_state_name(enum proc_state state);

#endif
