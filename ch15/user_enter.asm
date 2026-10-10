; spark -- user_enter.asm
;
; Going from ring 0 to ring 3, which C cannot express.
;
; There is no instruction that lowers the privilege level. The only way down
; is to return from an interrupt that never happened: iret pops eip, cs and
; eflags, and when the cs it pops has a lower privilege than the one running,
; it pops esp and ss as well and switches stacks. So the kernel builds the
; frame the processor would have pushed on its way in from ring 3, and
; returns through it.
;
; Chapter 12 fabricated a stack for a process that had never run. This is the
; same trick one ring further out.

bits 32
section .text

USER_CODE equ 0x1B            ; 0x18, low two bits set to 3
USER_DATA equ 0x23            ; 0x20, likewise
USER_FLAGS equ 0x202          ; the reserved bit 1, and IF

global user_enter
user_enter:
    mov eax, [esp + 4]        ; entry point
    mov ebx, [esp + 8]        ; top of the user stack

    ; The data segments are not part of the iret frame and have to be loaded
    ; by hand. The RPL in the selector is what makes them ring 3: the same
    ; descriptor at 0x20 would be a ring-0 view of the same bytes.
    mov cx, USER_DATA
    mov ds, cx
    mov es, cx
    mov fs, cx
    mov gs, cx

    push dword USER_DATA      ; ss
    push ebx                  ; esp
    push dword USER_FLAGS     ; eflags
    push dword USER_CODE      ; cs
    push eax                  ; eip

    iret
