/* spark -- kernel/program.c
 *
 * Ring 3, in about sixty lines.
 *
 * The only way out of here is int 0x80. There are no kernel functions to
 * call: a function name would link to a kernel address, the call would be
 * an ordinary jump, and it would land on a page ring 3 is not allowed to
 * execute. Every service these programs use has to be asked for.
 */
#include "program.h"
#include "syscall.h"

/* Both halves of a user program go in the section link.ld gives to ring 3:
 * the instructions, and the strings the instructions point at. A literal
 * left to itself lands in .rodata with the kernel's, on a page the program
 * is not allowed to read, and the write would be refused by its own kernel. */
#define USERTEXT __attribute__((section(".user.text")))
#define USERDATA __attribute__((section(".user.rodata")))

/* The user side of a system call. Three of these, differing only in how many
 * registers they set, because there are no default arguments in C and a
 * variadic version would need a stack the kernel would have to trust. */
USERTEXT static uint32_t call0(uint32_t number)
{
    uint32_t result;

    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "a" (number)
                      : "memory");
    return result;
}

USERTEXT static uint32_t call1(uint32_t number, uint32_t a)
{
    uint32_t result;

    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "a" (number), "b" (a)
                      : "memory");
    return result;
}

USERTEXT static uint32_t call2(uint32_t number,
                               uint32_t a, uint32_t b)
{
    uint32_t result;

    __asm__ volatile ("int $0x80"
                      : "=a" (result)
                      : "a" (number), "b" (a), "c" (b)
                      : "memory");
    return result;
}

USERDATA static const char greeting[] =
    "hello from ring 3, through int 0x80 and nothing else\n";

USERDATA static const char second[] =
    "  a second call, to show the first was not a fluke\n";

USERTEXT void user_hello(void)
{
    call2(SYS_WRITE, (uint32_t) (uintptr_t) greeting,
          sizeof greeting - 1);
    call2(SYS_WRITE, (uint32_t) (uintptr_t) second,
          sizeof second - 1);

    uint32_t pid = call0(SYS_GETPID);

    /* A pointer into the kernel, offered to the kernel as something to
     * print. The address is the first page of the kernel image, which this
     * program cannot read and the kernel can. */
    uint32_t bad = call2(SYS_WRITE, 0x00100000u, 16);

    /* The status carries the answer back: our pid if the kernel refused the
     * pointer, and zero if it followed it. */
    call1(SYS_EXIT, bad == SYS_ERROR ? pid : 0);
}

USERTEXT void user_cli(void)
{
    /* cli is not available at ring 3. The processor checks the current ring
     * against the I/O privilege level in eflags, which is 0, and raises a
     * general protection fault instead of executing it. */
    __asm__ volatile ("cli");

    call1(SYS_EXIT, 0);
}

USERTEXT void user_poke(void)
{
    /* The first page of the kernel image. It is mapped, and it is mapped
     * without PAGE_USER, so this store faults rather than succeeding. */
    volatile uint32_t *kernel_page =
        (volatile uint32_t *) 0x00100000u;

    *kernel_page = 0xdeadbeef;

    call1(SYS_EXIT, 0);
}

USERTEXT void user_fake_fault(void)
{
    /* Raising the page fault vector by hand, to see whether a program can
     * enter the kernel through a gate that was not meant for it. */
    __asm__ volatile ("int $0x0e");

    call1(SYS_EXIT, 0);
}
