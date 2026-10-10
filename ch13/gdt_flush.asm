; spark -- gdt_flush.asm
;
; Load the descriptor table and reload every segment register from it.
;
;     void gdt_flush(const struct gdt_ptr *ptr);
;
; Written in assembly because C has no syntax for loading a segment register
; and no expression that sets cs and eip together, which is what a far jump
; does. Inline assembly could issue both, but the sequence must stay in this
; order with nothing scheduled between its parts.

bits 32
section .text
global gdt_flush

; These must match the selectors in kernel/gdt.h. gdt_init reads cs and ds
; back after this call, so a disagreement is reported rather than left to
; cause a fault later.
KERNEL_CODE equ 0x08
KERNEL_DATA equ 0x10

gdt_flush:
    mov eax, [esp + 4]        ; the argument, a gdt_ptr *
    lgdt [eax]

    ; The five data segment registers take effect as they are written.
    mov ax, KERNEL_DATA
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; cs cannot be written directly: the code segment and the instruction
    ; pointer have to change together, or the processor would fetch the next
    ; instruction through one of them and not the other. A far jump sets
    ; both in one step.
    jmp KERNEL_CODE:.reload
.reload:
    ret
