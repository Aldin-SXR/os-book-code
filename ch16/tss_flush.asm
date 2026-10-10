; spark -- tss_flush.asm
;
; ltr loads the task register, and there is no way to write it from C: it
; takes a selector in a register or in memory and has no operand form the
; compiler can generate. One instruction, and it has to be here.

bits 32
section .text

global tss_flush
tss_flush:
    mov ax, [esp + 4]
    ltr ax
    ret
