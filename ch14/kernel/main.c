/* spark -- kernel/main.c
 *
 * The wall goes up.
 *
 * Every chapter so far has run everything in ring 0, where any instruction
 * is allowed and any address is reachable. This one draws a line through the
 * machine, puts a program on the far side of it, and then watches three more
 * programs try to cross.
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
#include "tss.h"
#include "user.h"
#include "syscall.h"
#include "program.h"

#define RULE_WIDTH 62
#define HZ         100

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

/* Run one process to completion and report how it ended.
 *
 * Every one of these is started the same way and reaches ring 3 the same
 * way. What differs is only what it does once it is there. */
static struct process *run(const char *label, void (*fn)(void))
{
    struct process *p = proc_create_user(label, fn);

    if (p == NULL) {
        panic("kmain: no room for a user process");
    }

    while (p->state != PROC_DONE) {
        proc_yield();
    }

    kprintf("  ");
    pad(label, 18);

    if (p->killed != NULL) {
        kprintf("killed: %s at 0x%08x, error 0x%02x\n",
                p->killed, p->fault_addr, p->exit_status);
    } else {
        kprintf("exited with status %u\n", p->exit_status);
    }

    return p;
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 14: user mode and system calls\n");
    vga_set_colour(vga_colour(VGA_LIGHT_GREY, VGA_BLACK));
    rule();

    gdt_init();
    tss_init();
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
    user_init();
    syscall_init();

    kprintf("\nthe wall      0x%08x to 0x%08x is all ring 3 may read\n",
            user_text_base(), user_text_end());
    kprintf("the gate      vector 0x%02x at dpl 3, and nothing else\n",
            SYSCALL_VECTOR);
    rule();

    /* --- one program that plays by the rules --- */
    kprintf("\n");

    struct process *hello = run("hello", user_hello);

    rule();

    kprintf("\nring 0        cs 0x%02x  ss 0x%02x\n",
            gdt_read_cs(), gdt_read_ds());
    kprintf("ring 3        cs 0x%02x  ss 0x%02x, the same two segments\n",
            syscall_user_cs(), syscall_user_ss());
    kprintf("its stack     esp0 0x%08x, where its traps landed\n",
            (uint32_t) (uintptr_t) hello->stack + PROC_STACK_BYTES);
    kprintf("              its status was its own pid, so the kernel\n");
    kprintf("              refused the kernel pointer it was offered\n");
    rule();

    /* --- three that do not --- */
    kprintf("\nthree programs that try to cross the line:\n");
    run("cli", user_cli);
    run("store to kernel", user_poke);
    run("int 0x0e", user_fake_fault);
    rule();

    kprintf("\nsyscalls      %u made, %u refused\n",
            syscall_count(), syscall_refused());
    kprintf("the kernel    still running, and never in any danger\n");
    rule();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
