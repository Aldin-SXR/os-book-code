; spark -- boot.asm
;
; The entry point for both ways of starting spark: a Multiboot loader such as
; GRUB or QEMU's own, and spark's boot sector.
;
; _start must be the first thing in the image. A Multiboot loader reads the
; ELF header and jumps to the entry point recorded there, so its position is
; immaterial. The boot sector has no ELF parser: it copies a flat image into
; memory and jumps at its first byte, which therefore has to be an
; instruction. Placing .text.entry ahead of .multiboot in link.ld is what
; guarantees that.

MBALIGN  equ 1 << 0              ; page-align loaded modules
MEMINFO  equ 1 << 1              ; request a memory map
MBFLAGS  equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002          ; fixed by the specification
CHECKSUM equ -(MAGIC + MBFLAGS)  ; the three must sum to zero

section .text.entry
global _start
extern kmain

_start:
    mov esp, stack_top

    ; The loader leaves its magic number in eax and the address of its
    ; information structure in ebx. The C calling convention passes arguments
    ; on the stack rightmost first, so ebx is pushed before eax.
    push ebx
    push eax
    call kmain

.hang:
    cli
    hlt
    jmp .hang

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

section .bss
align 16
stack_bottom:
    resb 16384
stack_top:
