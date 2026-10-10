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
#include "fs.h"
#include "string.h"

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

    /* Where the bytes go is not the program's business, and the program
     * has no way to ask. It called SYS_WRITE; the kernel decides whether
     * that reaches the screen or a file, and the shell decided that before
     * this process ran a single instruction. */
    uint32_t out = proc_current()->out_inode;

    if (out != 0) {
        return fs_append(out, s, len);
    }

    for (uint32_t i = 0; i < len; i++) {
        vga_putchar(s[i]);
    }

    return len;
}

/* Read a whole file by name.
 *
 * A real system opens a file once, gets a small number back, and reads
 * through that, so that a name is resolved once rather than on every call
 * and so that a program can hold a file open that has since been renamed.
 * spark resolves the path every time, which is simpler and slower, and
 * Chapter 17 names what a descriptor buys. */
static uint32_t sys_read(uint32_t path, uint32_t buf, uint32_t size)
{
    char name[64];

    if (!user_may_read(path, 1) || !user_may_read(buf, size)) {
        refused++;
        return SYS_ERROR;
    }

    const char *from = (const char *) (uintptr_t) path;
    uint32_t n = 0;

    while (n + 1 < sizeof name && from[n] != '\0') {
        name[n] = from[n];
        n++;
    }

    name[n] = '\0';

    uint32_t inum = fs_lookup(name);

    if (inum == 0) {
        refused++;
        return SYS_ERROR;
    }

    return fs_read(inum, 0, (void *) (uintptr_t) buf, size);
}

/* One directory entry of the root, by position. */
static uint32_t sys_list(uint32_t index, uint32_t namebuf)
{
    char name[FS_NAME_MAX];
    uint32_t inum;

    if (!user_may_read(namebuf, FS_NAME_MAX)) {
        refused++;
        return SYS_ERROR;
    }

    if (!fs_list(FS_ROOT_INODE, index, name, &inum)) {
        return 0;
    }

    memcpy((void *) (uintptr_t) namebuf, name, FS_NAME_MAX);
    return inum;
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

    case SYS_READ:
        regs->eax = sys_read(regs->ebx, regs->ecx, regs->edx);
        break;

    case SYS_LIST:
        regs->eax = sys_list(regs->ebx, regs->ecx);
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
