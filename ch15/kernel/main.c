/* spark -- kernel/main.c
 *
 * A disk, and the agreement that turns it into files.
 *
 * Every chapter so far has kept everything in memory, where it vanishes when
 * the machine stops. This one reads a disk, finds a file system on it, and
 * shows the one thing about file systems that surprises people: a name and a
 * file are separate objects, and deleting a name is not deleting a file.
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
#include "string.h"

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

/* Print a file, which is the shortest program anybody writes. */
static void show(const char *path)
{
    char buf[128];
    uint32_t inum = fs_lookup(path);
    uint32_t got = fs_read(inum, 0, buf, sizeof buf - 1);

    /* The last newline is dropped, because the caller decides where the
     * line after this one starts. */
    while (got > 0 && buf[got - 1] == '\n') {
        got--;
    }

    buf[got] = '\0';

    for (uint32_t i = 0; i < got; i++) {
        if (buf[i] == '\n') {
            kprintf("\n  ");
        } else {
            vga_putchar(buf[i]);
        }
    }
}

/* How many names point at this path's inode, and how big it is. */
static void report(const char *label, const char *path)
{
    struct fs_inode node;
    uint32_t inum = fs_lookup(path);

    kprintf("  ");
    pad(label, 22);

    if (inum == 0 || !fs_stat(inum, &node)) {
        kprintf("no such name\n");
        return;
    }

    kprintf("inode %u, %u bytes, %u link(s)\n",
            inum, node.size, node.links);
}

void kmain(uint32_t magic, uint32_t boot_info)
{
    vga_init();
    vga_set_colour(vga_colour(VGA_WHITE, VGA_BLACK));
    kprintf("spark, chapter 15: a file system\n");
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

    if (!ata_init()) {
        panic("kmain: no disk on the primary slave");
    }

    if (!fs_mount()) {
        panic("kmain: that disk is not sparkfs");
    }

    kprintf("\nthe disk      %u sectors of %u bytes, and it said so itself\n",
            ata_sectors(), ATA_SECTOR_BYTES);
    kprintf("sparkfs       %u inodes, %u blocks, %u in use\n",
            FS_INODES, fs_blocks(), fs_blocks_used());
    rule();

    /* --- the directory, which is a file like any other --- */
    kprintf("\n/ holds:\n");

    char name[FS_NAME_MAX];
    uint32_t inum;
    struct fs_inode node;

    for (uint32_t i = 0; fs_list(FS_ROOT_INODE, i, name, &inum); i++) {
        fs_stat(inum, &node);
        kprintf("  ");
        pad(name, 22);
        kprintf("inode %u, %s, %u bytes\n", inum,
                node.type == FS_DIR ? "dir " : "file", node.size);
    }
    rule();

    kprintf("\n/readme.txt says:\n  ");
    show("/readme.txt");

    /* Two names deep, so the lookup has to read the root, find docs, read
     * that, and find hello.txt in it. A directory is a file. */
    kprintf("\n/docs/hello.txt says: ");
    show("/docs/hello.txt");

    /* --- the point of the chapter --- */
    kprintf("\n\na name is not a file:\n");
    report("/readme.txt", "/readme.txt");

    if (!fs_link("/readme.txt", "copy.txt")) {
        panic("kmain: could not link");
    }

    report("after the link", "/copy.txt");

    if (!fs_unlink("readme.txt")) {
        panic("kmain: could not unlink");
    }

    report("/readme.txt gone", "/readme.txt");
    report("/copy.txt still", "/copy.txt");
    rule();

    /* --- and back to where we started --- */
    uint32_t before = fs_blocks_used();

    if (fs_create("new.txt", "written by the kernel, onto a disk\n", 35) == 0) {
        panic("kmain: could not create");
    }

    kprintf("\nwrote /new.txt, and the blocks went %u -> %u\n",
            before, fs_blocks_used());
    kprintf("reading it back: ");
    show("/new.txt");

    /* And the other half of a link count: this name is the only one, so
     * removing it removes the file and the block comes back. */
    uint32_t after = fs_blocks_used();

    if (!fs_unlink("new.txt")) {
        panic("kmain: could not unlink new.txt");
    }

    kprintf("\nunlinked it again, and they went %u -> %u\n",
            after, fs_blocks_used());
    kprintf("disk          %u reads and %u writes to get here\n",
            ata_reads(), ata_writes());
    rule();

    for (;;) {
        __asm__ volatile ("hlt");
    }
}
