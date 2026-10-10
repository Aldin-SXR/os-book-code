/* spark -- user/link.ld
 *
 * The layout of a program that lives on the disk.
 *
 * A kernel image is loaded by something that reads ELF headers. These are
 * flat binaries: the loader in kernel/exec.c copies the file to one address
 * and jumps at its first byte, so the first byte has to be an instruction
 * and every address in the program has to be fixed in advance.
 */
ENTRY(_start)

SECTIONS
{
    /* Well above the direct map of Chapter 11 and well below the heap. Every
     * program is linked for this one address, because spark loads one at a
     * time and has no relocation. */
    . = 0x50000000;

    .text : {
        KEEP(*(.text.start))    /* first, so it is byte 0 */
        *(.text*)
    }

    .rodata : { *(.rodata*) }
    .data   : { *(.data*)   }

    /* Last, so that the loader can zero everything past the end of the file
     * and reach all of it. --oformat=binary leaves .bss out of the file. */
    .bss : {
        *(COMMON)
        *(.bss*)
    }
    _end = .;
}
