/* spark -- link.ld
 *
 * The physical layout of the kernel image. Without a script the linker would
 * lay the program out for a hosted environment, which assumes the operating
 * system this code is replacing.
 */
ENTRY(_start)

SECTIONS
{
    /* The first megabyte is already committed: real-mode interrupt vectors,
     * BIOS data and the VGA buffer. The kernel is placed above all of it. */
    . = 1M;

    /* Two symbols with no contents, marking the ends of the image. The
     * frame allocator reads them to reserve the memory the kernel occupies.
     * Only the linker knows where that ends, because .bss grows with every
     * global the kernel adds. */
    _kernel_start = .;

    /* .text.entry comes first and .multiboot straight after it, so the
     * Multiboot header sits inside the first 8 KiB, where a loader looks. */
    .text ALIGN(4K) :
    {
        /* .text.entry holds _start alone and is placed first so that the
         * first byte of the image is an instruction: the boot sector jumps
         * straight at it. A Multiboot loader reads the ELF entry point and
         * is indifferent to the order. */
        KEEP(*(.text.entry))
        KEEP(*(.multiboot))
        *(.text*)
    }

    /* The wildcards are required. A compiler splits read-only data across
     * .rodata.str1.1, .rodata.cst4 and others; a rule naming only .rodata
     * leaves those for the linker to place, which can put them in front of
     * the Multiboot header. */
    /* Everything ring 3 is allowed to see, on pages of its own.
     *
     * The boundary this book is about has to be drawn somewhere in the
     * address space, and a section is how you draw it: vmm_map can only
     * grant user access a page at a time, so the user program and the
     * strings it prints have to start and end on page boundaries with no
     * kernel data sharing a page with them. */
    .user ALIGN(4K) :
    {
        _user_start = .;
        *(.user.text)
        *(.user.rodata)
        . = ALIGN(4K);
        _user_end = .;
    }

    .rodata ALIGN(4K) : { *(.rodata*) }
    .data   ALIGN(4K) : { *(.data*)   }

    /* .bss is zero at start-up, so it occupies memory but no space in the
     * file. The stack reserved in boot.asm lives here. */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }

    _kernel_end = ALIGN(4K);
}
