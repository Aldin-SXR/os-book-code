/* spark -- kernel/main.c
 *
 * A person types a word and a program runs.
 *
 * Every chapter of this book has been building towards one line of that
 * shape. The screen, the keyboard, the memory, the processes, the ring and
 * the disk were each necessary and none of them was the point. This is.
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
#include "tss.h"
#include "user.h"
#include "syscall.h"
#include "ata.h"
#include "fs.h"
#include "exec.h"
#include "shell.h"

#define RULE_WIDTH 62
#define HZ         100

static void rule(void)
{
    for (int i = 0; i < RULE_WIDTH; i++) {
        vga_putchar('-');
    }
}

/* The shell is a process like any other, which is the sentence this whole
 * chapter is about. It is created here and then nothing supervises it. */
static void shell_process(void)
{
    /* Four commands, run before anybody has typed, so that a reader who is
     * looking at a screenshot sees the same thing as a reader at a
     * keyboard. Everything after this comes from the keys. */
    shell_run("echo hello from a program on the disk", true);
    shell_run("wc /readme.txt", true);
    shell_run("echo written by echo > /out.txt", true);
    shell_run("cat /out.txt", true);

    /* Two commands that touched no disk at all. They matter more than they
     * look: on the core path, where Chapter 15 is an extension, commands of
     * this shape are the whole shell. */
    shell_run("uptime", true);
    shell_run("mem", true);
    rule();

    kprintf("\nloaded        %u bytes to 0x%08x, and ran it\n",
            exec_last_size(), EXEC_BASE);
    kprintf("processes     %u so far, and the shell is still pid %u\n",
            proc_count(), proc_current()->pid);
    rule();
    kprintf("\n");

    shell_loop();
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 16: the shell\n");
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

    if (!ata_init() || !fs_mount()) {
        panic("kmain: no sparkfs on the primary slave");
    }

    exec_init();

    kprintf("\n/bin holds   ");

    char name[FS_NAME_MAX];
    uint32_t inum;
    uint32_t bin = fs_lookup("/bin");

    for (uint32_t i = 0; fs_list(bin, i, name, &inum); i++) {
        kprintf("%s ", name);
    }

    kprintf("\n");
    rule();
    kprintf("\n");

    if (proc_create("sh", shell_process) == NULL) {
        panic("kmain: no room for a shell");
    }

    /* And now the kernel has nothing left to do. It yields to the shell and
     * spends the rest of its life being yielded back to. */
    for (;;) {
        proc_yield();
        __asm__ volatile ("hlt");
    }
}
