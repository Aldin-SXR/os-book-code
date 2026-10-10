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

    /* .multiboot comes first, and first in the file, so the boot loader finds
     * the header inside the 8 KiB it is willing to search for it. */
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
    .rodata ALIGN(4K) : { *(.rodata*) }
    .data   ALIGN(4K) : { *(.data*)   }

    /* .bss is zero at start-up, so it occupies memory but no space in the
     * file. The stack reserved in boot.asm lives here. */
    .bss ALIGN(4K) :
    {
        *(COMMON)
        *(.bss*)
    }
}
