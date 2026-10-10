/* spark -- kernel/syscall.c
 *
 * What happens between int 0x80 and the value that comes back in eax.
 */
#include "syscall.h"
#include "user.h"
#include "proc.h"
#include "sched.h"
#include "vga.h"
#include "printf.h"

static uint32_t calls;
static uint32_t refused;
static uint32_t first_cs;
static uint32_t first_ss;

/* Print a run of bytes the user asked us to print.
 *
 * Everything difficult about system calls is in the first three lines. The
 * kernel is running with the privilege to read any address in the machine,
 * and it has just been handed an address by a program that has no such
 * privilege. Following it without asking is how a program reads the page
 * tables, or the keyboard buffer, or another process's stack, by passing a
 * pointer to them and being told what is there. */
static uint32_t sys_write(uint32_t text, uint32_t len)
{
    if (!user_may_read(text, len)) {
        refused++;
        return SYS_ERROR;
    }

    const char *s = (const char *) (uintptr_t) text;

    for (uint32_t i = 0; i < len; i++) {
        vga_putchar(s[i]);
    }

    return len;
}

/* The dispatcher.
 *
 * regs->eax is the number the program put there, and writing to regs->eax is
 * how the result gets back: the frame is on this process's kernel stack, and
 * popa in isr_common loads eax from it on the way out. */
static void syscall_handler(struct registers *regs)
{
    calls++;

    /* The frame carries ss only because the caller was at a lower
     * privilege, which is the same condition isr_from_user tests. */
    if (first_cs == 0 && isr_from_user(regs)) {
        first_cs = regs->cs;
        first_ss = regs->ss;
    }

    switch (regs->eax) {
    case SYS_WRITE:
        regs->eax = sys_write(regs->ebx, regs->ecx);
        break;

    case SYS_EXIT:
        /* Does not return. proc_exit marks this process finished and hands
         * the processor away for good, so the iret at the end of
         * isr_common is never reached on this stack. */
        proc_current()->exit_status = regs->ebx;
        proc_exit();
        break;

    case SYS_GETPID:
        regs->eax = proc_current()->pid;
        break;

    case SYS_YIELD:
        proc_yield();
        regs->eax = 0;
        break;

    case SYS_SLEEP:
        sched_sleep(regs->ebx);
        regs->eax = 0;
        break;

    default:
        /* An unknown number is refused rather than ignored. A kernel that
         * returns success for a call it did not make is worse than one that
         * fails, because the program carries on believing something. */
        refused++;
        regs->eax = SYS_ERROR;
        break;
    }
}

void syscall_init(void)
{
    calls = 0;
    refused = 0;
    first_cs = 0;
    first_ss = 0;
    isr_install(SYSCALL_VECTOR, syscall_handler);
}

uint32_t syscall_count(void)   { return calls; }
uint32_t syscall_refused(void) { return refused; }
uint32_t syscall_user_cs(void) { return first_cs; }
uint32_t syscall_user_ss(void) { return first_ss; }
