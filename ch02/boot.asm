; spark -- boot.asm
;
; The first of spark's code that the machine runs. Three jobs, in order:
; identify the kernel to the boot loader, establish a stack, and enter C.

; The Multiboot header. A boot loader scans the first 8 KiB of the kernel
; file for MAGIC; the flags state what the kernel requires of it, and the
; checksum distinguishes a real header from twelve bytes that happen to
; match.
MBALIGN  equ 1 << 0              ; load modules on page boundaries
MEMINFO  equ 1 << 1              ; request a memory map
MBFLAGS  equ MBALIGN | MEMINFO
MAGIC    equ 0x1BADB002          ; fixed by the Multiboot specification
CHECKSUM equ -(MAGIC + MBFLAGS)  ; the three must sum to zero

section .multiboot
align 4
    dd MAGIC
    dd MBFLAGS
    dd CHECKSUM

; The stack. No C code can run without one and the loader provides none.
; 16 KiB, in .bss so that it occupies no space in the image, and aligned to
; 16 bytes: the 32-bit System V ABI asks for that at every call, and clang's
; bare-metal target, which only assumes 4, is satisfied by it too.
section .bss
align 16
stack_bottom:
    resb 16384
stack_top:

section .text
global _start
extern kmain

_start:
    ; The Multiboot specification leaves esp undefined, so the kernel sets
    ; its own. The stack grows downwards and therefore starts at the top.
    mov esp, stack_top

    call kmain

    ; kmain is not expected to return. Halt rather than execute whatever
    ; bytes follow it.
.hang:
    cli                          ; no interrupts
    hlt                          ; sleep until one arrives, which it cannot
    jmp .hang                    ; unless one does anyway
